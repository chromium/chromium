// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/insecure_download_blocking.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/download/public/common/download_stats.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using download::DownloadSource;
using InsecureDownloadStatus = download::DownloadItem::InsecureDownloadStatus;

namespace {

// Configuration for which extensions to warn/block. These parameters are set
// differently for testing, so the listed defaults are only used when the flag
// is manually enabled (and in unit tests).
//
// Extensions must be in lower case! Extensions are compared against save path
// determined by Chrome prior to the user seeing a file picker.
//
// The extension list for each type (warn, block, silent block) can be
// configured in two ways: as an allowlist, or as a blocklist. When the
// extension list is a blocklist, extensions listed will trigger a
// warning/block. If the extension list is configured as an allowlist, all
// extensions EXCEPT those listed will trigger a warning/block.
//
// To make manual testing easier, the defaults are to have a small blocklist for
// block/silent block, and a small allowlist for warnings. This means that
// every mixed content download will at *least* generate a warning.
const base::FeatureParam<bool> kTreatSilentBlockListAsAllowlist(
    &features::kTreatUnsafeDownloadsAsActive,
    "TreatSilentBlockListAsAllowlist",
    true);
const base::FeatureParam<std::string> kSilentBlockExtensionList(
    &features::kTreatUnsafeDownloadsAsActive,
    "SilentBlockExtensionList",
    "silently_unblocked_for_testing");

const base::FeatureParam<bool> kTreatBlockListAsAllowlist(
    &features::kTreatUnsafeDownloadsAsActive,
    "TreatBlockListAsAllowlist",
    false);
const base::FeatureParam<std::string> kBlockExtensionList(
    &features::kTreatUnsafeDownloadsAsActive,
    "BlockExtensionList",
    "");

// Note: this is an allowlist, so acts as a catch-all.
const base::FeatureParam<bool> kTreatWarnListAsAllowlist(
    &features::kTreatUnsafeDownloadsAsActive,
    "TreatWarnListAsAllowlist",
    false);
const base::FeatureParam<std::string> kWarnExtensionList(
    &features::kTreatUnsafeDownloadsAsActive,
    "WarnExtensionList",
    "");

// Map the string file extension to the corresponding histogram enum.
InsecureDownloadExtensions GetExtensionEnumFromString(
    const std::string& extension) {
  if (extension.empty())
    return InsecureDownloadExtensions::kNone;

  auto lower_extension = base::ToLowerASCII(extension);
  for (auto candidate : kExtensionsToEnum) {
    if (candidate.extension == lower_extension)
      return candidate.value;
  }
  return InsecureDownloadExtensions::kUnknown;
}

// Get the appropriate histogram metric name for the initiator/download security
// state combo.
std::string GetDownloadBlockingExtensionMetricName(
    InsecureDownloadSecurityStatus status) {
  switch (status) {
    case InsecureDownloadSecurityStatus::kInitiatorUnknownFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorUnknown,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorUnknownFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorUnknown,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorSecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorSecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInsecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInsecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInsecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredSecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredSecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredInsecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredInsecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredInsecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredInsecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kDownloadIgnored:
      NOTREACHED();
  }
  NOTREACHED();
  return std::string();
}

// Get appropriate enum value for the initiator/download security state combo
// for histogram reporting. |dl_secure| signifies whether the download was
// a secure source. |inferred| is whether the initiator value is our best guess.
InsecureDownloadSecurityStatus GetDownloadBlockingEnum(
    absl::optional<url::Origin> initiator,
    bool dl_secure,
    bool inferred) {
  if (inferred) {
    if (initiator->GetURL().SchemeIsCryptographic()) {
      if (dl_secure) {
        return InsecureDownloadSecurityStatus::
            kInitiatorInferredSecureFileSecure;
      }
      return InsecureDownloadSecurityStatus::
          kInitiatorInferredSecureFileInsecure;
    }

    if (dl_secure) {
      return InsecureDownloadSecurityStatus::
          kInitiatorInferredInsecureFileSecure;
    }
    return InsecureDownloadSecurityStatus::
        kInitiatorInferredInsecureFileInsecure;
  }

  if (!initiator.has_value()) {
    if (dl_secure)
      return InsecureDownloadSecurityStatus::kInitiatorUnknownFileSecure;
    return InsecureDownloadSecurityStatus::kInitiatorUnknownFileInsecure;
  }

  if (initiator->GetURL().SchemeIsCryptographic()) {
    if (dl_secure)
      return InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure;
    return InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure;
  }

  if (dl_secure)
    return InsecureDownloadSecurityStatus::kInitiatorInsecureFileSecure;
  return InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure;
}

struct InsecureDownloadData {
  InsecureDownloadData(const base::FilePath& path,
                       const download::DownloadItem* item)
      : item_(item) {
    // Configure initiator.
    bool initiator_inferred = false;
    initiator_ = item->GetRequestInitiator();
    if (!initiator_.has_value() && item->GetTabUrl().is_valid()) {
      initiator_inferred = true;
      initiator_ = url::Origin::Create(item->GetTabUrl());
    }

    // Extract extension.
#if BUILDFLAG(IS_WIN)
    extension_ = base::WideToUTF8(path.FinalExtension());
#else
    extension_ = path.FinalExtension();
#endif
    if (!extension_.empty()) {
      DCHECK_EQ(extension_[0], '.');
      extension_ = extension_.substr(1);  // Omit leading dot.
    }

    // Evaluate download security.
    is_redirect_chain_secure_ = true;
    // Skip over the final URL so that we can investigate it separately below.
    // The redirect chain always contains the final URL, so this is always safe
    // in Chrome, but some tests don't plan for it, so we check here.
    const auto& chain = item->GetUrlChain();
    if (chain.size() > 1) {
      for (unsigned i = 0; i < chain.size() - 1; ++i) {
        const GURL& url = chain[i];
        if (!network::IsUrlPotentiallyTrustworthy(url)) {
          is_redirect_chain_secure_ = false;
          break;
        }
      }
    }
    const GURL& dl_url = item->GetURL();
    // Whether or not the download was securely delivered, ignoring where we got
    // the download URL from (i.e. ignoring the initiator).
    bool download_delivered_securely =
        is_redirect_chain_secure_ &&
        (network::IsUrlPotentiallyTrustworthy(dl_url) ||
         dl_url.SchemeIsBlob() || dl_url.SchemeIsFile());

    // Configure mixed content status.
    // Some downloads don't qualify for blocking, and are thus never
    // mixed-content. At a minimum, this includes:
    //  - retries/reloads (since the original DL would have been blocked, and
    //    initiating context is lost on retry anyway),
    //  - anything triggered directly from the address bar or similar.
    //  - internal-Chrome downloads (e.g. downloading profile photos),
    //  - webview/CCT,
    //  - anything extension related,
    //  - etc.
    //
    // TODO(1029062): INTERNAL_API is also used for background fetch. That
    // probably isn't the correct behavior, since INTERNAL_API is otherwise used
    // for Chrome stuff. Background fetch should probably be HTTPS-only.
    auto download_source = item->GetDownloadSource();
    auto transition_type = item->GetTransitionType();
    if (download_source == DownloadSource::RETRY ||
        (transition_type & ui::PAGE_TRANSITION_RELOAD) ||
        (transition_type & ui::PAGE_TRANSITION_TYPED) ||
        (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
        (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK) ||
        (transition_type & ui::PAGE_TRANSITION_AUTO_TOPLEVEL) ||
        (transition_type & ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
        (transition_type & ui::PAGE_TRANSITION_FROM_API) ||
        download_source == DownloadSource::OFFLINE_PAGE ||
        download_source == DownloadSource::INTERNAL_API ||
        download_source == DownloadSource::EXTENSION_API ||
        download_source == DownloadSource::EXTENSION_INSTALLER) {
      base::UmaHistogramEnumeration(
          kInsecureDownloadHistogramName,
          InsecureDownloadSecurityStatus::kDownloadIgnored);
      is_mixed_content_ = false;
    } else {  // Not ignorable download.
      // Record some metrics first.
      auto security_status = GetDownloadBlockingEnum(
          initiator_, download_delivered_securely, initiator_inferred);
      base::UmaHistogramEnumeration(
          GetDownloadBlockingExtensionMetricName(security_status),
          GetExtensionEnumFromString(extension_));
      base::UmaHistogramEnumeration(kInsecureDownloadHistogramName,
                                    security_status);
      download::RecordDownloadValidationMetrics(
          download::DownloadMetricsCallsite::kMixContentDownloadBlocking,
          download::CheckDownloadConnectionSecurity(item->GetURL(),
                                                    item->GetUrlChain()),
          download::DownloadContentFromMimeType(item->GetMimeType(), false));

      is_mixed_content_ = (initiator_.has_value() &&
                           initiator_->GetURL().SchemeIsCryptographic() &&
                           !download_delivered_securely);
    }

    // Configure insecure download status.
    // Exclude download sources needed by Chrome from blocking. While this is
    // similar to MIX-DL above, it intentionally blocks more user-initiated
    // downloads. For example, downloads are blocked even if they're initiated
    // from the omnibox.
    if (download_source == DownloadSource::RETRY ||
        (transition_type & ui::PAGE_TRANSITION_RELOAD) ||
        (transition_type & ui::PAGE_TRANSITION_FROM_API) ||
        download_source == DownloadSource::OFFLINE_PAGE ||
        download_source == DownloadSource::INTERNAL_API ||
        download_source == DownloadSource::EXTENSION_API ||
        download_source == DownloadSource::EXTENSION_INSTALLER) {
      is_insecure_download_ = false;
    } else {  // Not ignorable download.
      // TODO(crbug.com/1352598): Add blocking metrics.
      // insecure downloads are either delivered insecurely, or we can't trust
      // who told us to download them (i.e. have an insecure initiator).
      is_insecure_download_ = (initiator_.has_value() &&
                               !initiator_->GetURL().SchemeIsCryptographic()) ||
                              !download_delivered_securely;
    }
  }

  absl::optional<url::Origin> initiator_;
  std::string extension_;
  raw_ptr<const download::DownloadItem> item_;

  // Was the download redirected only through secure URLs?
  bool is_redirect_chain_secure_;
  // Was the download initiated by a secure origin, but delivered insecurely?
  bool is_mixed_content_;
  // Was the download initiated by an insecure origin or delivered insecurely?
  bool is_insecure_download_;
};

// Check if |extension| is contained in the comma separated |extension_list|.
bool ContainsExtension(const std::string& extension_list,
                       const std::string& extension) {
  for (const auto& item :
       base::SplitStringPiece(extension_list, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    DCHECK_EQ(base::ToLowerASCII(item), item);
    if (base::EqualsCaseInsensitiveASCII(extension, item))
      return true;
  }

  return false;
}

// Just print a descriptive message to the console about the blocked download.
// |is_blocked| indicates whether this download will be blocked now.
void PrintConsoleMessage(const InsecureDownloadData& data, bool is_blocked) {
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(data.item_);
  if (!web_contents) {
    return;
  }

  web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(
          "Mixed Content: The site at '%s' was loaded over a secure "
          "connection, but the file at '%s' was %s an insecure "
          "connection. This file should be served over HTTPS. "
          "This download %s. See "
          "https://blog.chromium.org/2020/02/"
          "protecting-users-from-insecure.html"
          " for more details.",
          data.initiator_->GetURL().spec().c_str(),
          data.item_->GetURL().spec().c_str(),
          (data.is_redirect_chain_secure_ ? "loaded over"
                                          : "redirected through"),
          (is_blocked ? "has been blocked"
                      : "will be blocked in future versions of Chrome")));
}

bool IsDownloadPermittedByContentSettings(
    Profile* profile,
    const absl::optional<url::Origin>& initiator) {
  // TODO(crbug.com/1048957): Checking content settings crashes unit tests on
  // Android. It shouldn't.
#if !BUILDFLAG(IS_ANDROID)
  ContentSettingsForOneType settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::MIXEDSCRIPT, &settings);

  // When there's only one rule, it's the default wildcard rule.
  if (settings.size() == 1) {
    DCHECK(settings[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(settings[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return settings[0].GetContentSetting() == CONTENT_SETTING_ALLOW;
  }

  for (const auto& setting : settings) {
    if (setting.primary_pattern.Matches(initiator->GetURL())) {
      return setting.GetContentSetting() == CONTENT_SETTING_ALLOW;
    }
  }
  NOTREACHED();
#endif

  return false;
}

}  // namespace

InsecureDownloadStatus GetInsecureDownloadStatusForDownload(
    Profile* profile,
    const base::FilePath& path,
    const download::DownloadItem* item) {
  InsecureDownloadData data(path, item);

  // When enabled, show a visible (bypassable) warning on insecure downloads.
  // Since mixed download blocking is more severe, exclude mixed downloads from
  // this early-return to let the mixed download logic below apply.
  if (base::FeatureList::IsEnabled(features::kBlockInsecureDownloads) &&
      data.is_insecure_download_ && !data.is_mixed_content_) {
    PrintConsoleMessage(data, true);
    return InsecureDownloadStatus::BLOCK;
  }

  if (!data.is_mixed_content_) {
    return InsecureDownloadStatus::SAFE;
  }

  // As of M81, print a console message even if no other blocking is enabled.
  if (!base::FeatureList::IsEnabled(features::kTreatUnsafeDownloadsAsActive)) {
    PrintConsoleMessage(data, false);
    return InsecureDownloadStatus::SAFE;
  }

  if (IsDownloadPermittedByContentSettings(profile, data.initiator_)) {
    PrintConsoleMessage(data, false);
    return InsecureDownloadStatus::SAFE;
  }

  if (ContainsExtension(kSilentBlockExtensionList.Get(), data.extension_) !=
      kTreatSilentBlockListAsAllowlist.Get()) {
    PrintConsoleMessage(data, true);

    // Only permit silent blocking when not initiated by an explicit user
    // action.  Otherwise, fall back to visible blocking.
    auto download_source = data.item_->GetDownloadSource();
    if (download_source == DownloadSource::CONTEXT_MENU ||
        download_source == DownloadSource::WEB_CONTENTS_API) {
      return InsecureDownloadStatus::BLOCK;
    }

    return InsecureDownloadStatus::SILENT_BLOCK;
  }

  if (ContainsExtension(kBlockExtensionList.Get(), data.extension_) !=
      kTreatBlockListAsAllowlist.Get()) {
    PrintConsoleMessage(data, true);
    return InsecureDownloadStatus::BLOCK;
  }

  if (ContainsExtension(kWarnExtensionList.Get(), data.extension_) !=
      kTreatWarnListAsAllowlist.Get()) {
    PrintConsoleMessage(data, true);
    return InsecureDownloadStatus::WARN;
  }

  // The download is still mixed content, but we're not blocking it yet.
  PrintConsoleMessage(data, false);
  return InsecureDownloadStatus::SAFE;
}

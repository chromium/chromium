// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/mixed_content_download_blocking.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/download/public/common/download_stats.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

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
    base::Optional<url::Origin> initiator,
    bool dl_secure) {
  if (!initiator.has_value()) {
    if (dl_secure)
      return kInsecureDownloadHistogramInitiatorUnknownTargetSecure;
    return kInsecureDownloadHistogramInitiatorUnknownTargetInsecure;
  }

  if (initiator->GetURL().SchemeIsCryptographic()) {
    if (dl_secure)
      return kInsecureDownloadHistogramInitiatorSecureTargetSecure;
    return kInsecureDownloadHistogramInitiatorSecureTargetInsecure;
  }

  if (dl_secure)
    return kInsecureDownloadHistogramInitiatorInsecureTargetSecure;
  return kInsecureDownloadHistogramInitiatorInsecureTargetInsecure;
}

// Get appropriate enum value for the initiator/download security state combo
// for histogram reporting.
InsecureDownloadSecurityStatus GetDownloadBlockingEnum(
    base::Optional<url::Origin> initiator,
    bool dl_secure) {
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

}  // namespace

bool ShouldBlockFileAsMixedContent(const base::FilePath& path,
                                   const download::DownloadItem& item) {
  // Extensions must be in lower case! Extensions are compared against save path
  // determined by Chrome prior to the user seeing a file picker.
  const std::vector<std::string> kDefaultUnsafeExtensions = {
      "exe", "scr", "msi", "vb",  "dmg", "pkg", "crx",
      "gz",  "zip", "bz2", "rar", "7z",  "tar",
  };

  // Evaluate download security
  const GURL& dl_url = item.GetURL();
  bool is_download_secure = content::IsOriginSecure(dl_url) ||
                            dl_url.SchemeIsBlob() || dl_url.SchemeIsFile();
  bool is_redirect_chain_secure = true;
  for (const auto& url : item.GetUrlChain()) {
    if (!content::IsOriginSecure(url)) {
      is_redirect_chain_secure = false;
      break;
    }
  }
  is_download_secure = is_download_secure && is_redirect_chain_secure;

  // Check field trials for override of the unsafe extensions.
  std::string field_trial_arg = base::GetFieldTrialParamValueByFeature(
      features::kTreatUnsafeDownloadsAsActive,
      features::kTreatUnsafeDownloadsAsActiveParamName);
  std::vector<base::StringPiece> unsafe_extensions = base::SplitStringPiece(
      field_trial_arg, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (unsafe_extensions.empty()) {
    unsafe_extensions.insert(unsafe_extensions.end(),
                             kDefaultUnsafeExtensions.begin(),
                             kDefaultUnsafeExtensions.end());
  }

  auto initiator = item.GetRequestInitiator();

  // Then see if that extension is blocked
  bool found_blocked_extension = false;
#if defined(OS_WIN)
  std::string extension(base::WideToUTF8(path.FinalExtension()));
#else
  std::string extension(path.FinalExtension());
#endif
  if (!extension.empty()) {
    extension = extension.substr(1);  // Omit leading dot.
    for (const auto& unsafe_extension : unsafe_extensions) {
      if (base::LowerCaseEqualsASCII(extension, unsafe_extension)) {
        found_blocked_extension = true;
        break;
      }
    }
  }

  base::UmaHistogramEnumeration(
      GetDownloadBlockingExtensionMetricName(initiator, is_download_secure),
      GetExtensionEnumFromString(extension));
  base::UmaHistogramEnumeration(
      kInsecureDownloadHistogramName,
      GetDownloadBlockingEnum(initiator, is_download_secure));
  download::RecordDownloadValidationMetrics(
      download::DownloadMetricsCallsite::kMixContentDownloadBlocking,
      download::CheckDownloadConnectionSecurity(dl_url, item.GetUrlChain()),
      download::DownloadContentFromMimeType(item.GetMimeType(), false));

  if (!(initiator.has_value() && initiator->GetURL().SchemeIsCryptographic() &&
        !is_download_secure && found_blocked_extension &&
        base::FeatureList::IsEnabled(
            features::kTreatUnsafeDownloadsAsActive))) {
    return false;
  }

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(&item);
  if (web_contents) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Mixed Content: The site at '%s' was loaded over a secure "
            "connection, but the file at '%s' was %s an insecure "
            "connection. This file should be served over HTTPS.",
            initiator->GetURL().spec().c_str(), item.GetURL().spec().c_str(),
            (is_redirect_chain_secure ? "loaded over" : "redirected through")));
  }

  return true;
}

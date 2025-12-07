// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_item_metadata.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/download_item_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#endif

namespace safe_browsing {

namespace {

#if BUILDFLAG(IS_ANDROID)
// File suffix for APKs.
const base::FilePath::CharType kApkSuffix[] = FILE_PATH_LITERAL(".apk");
#endif

int ArchiveEntryWeight(const ClientDownloadRequest::ArchivedBinary& entry) {
  return FileTypePolicies::GetInstance()
      ->SettingsForFile(base::FilePath::FromUTF8Unsafe(entry.file_path()),
                        GURL{}, nullptr)
      .file_weight();
}

size_t ArchiveEntryDepth(const ClientDownloadRequest::ArchivedBinary& entry) {
  return base::FilePath::FromUTF8Unsafe(entry.file_path())
      .GetComponents()
      .size();
}

void SelectEncryptedEntry(
    std::vector<ClientDownloadRequest::ArchivedBinary>* considering,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        selected) {
  auto it = std::ranges::find_if(
      *considering, &ClientDownloadRequest::ArchivedBinary::is_encrypted);
  if (it != considering->end()) {
    *selected->Add() = *it;
    considering->erase(it);
  }
}

void SelectDeepestEntry(
    std::vector<ClientDownloadRequest::ArchivedBinary>* considering,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        selected) {
  auto it = std::ranges::max_element(*considering, {}, &ArchiveEntryDepth);
  if (it != considering->end()) {
    *selected->Add() = *it;
    considering->erase(it);
  }
}

void SelectWildcardEntryAtFront(
    std::vector<ClientDownloadRequest::ArchivedBinary>* considering,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        selected) {
  int remaining_executables = std::ranges::count_if(
      *considering, &ClientDownloadRequest::ArchivedBinary::is_executable);
  for (auto it = considering->begin(); it != considering->end(); ++it) {
    if (it->is_executable()) {
      // Choose the current entry with probability 1/remaining_executables. This
      // leads to a uniform distribution over all executables.
      if (remaining_executables * base::RandDouble() < 1) {
        *selected->Add() = *it;
        // Move the selected entry to the front. There's no easy way to insert
        // at a specific location in a RepeatedPtrField, so we do the move as a
        // series of swaps.
        for (int i = 0; i < selected->size() - 1; ++i) {
          selected->SwapElements(i, selected->size() - 1);
        }
        considering->erase(it);
        return;
      }

      --remaining_executables;
    }
  }
}

SafeBrowsingNavigationObserverManager* GetNavigationObserverManager(
    content::WebContents* web_contents) {
  return SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
}

void AddEventUrlToReferrerChain(const download::DownloadItem& item,
                                content::RenderFrameHost* render_frame_host,
                                ReferrerChain* out_referrer_chain) {
  ReferrerChainEntry* event_url_entry = out_referrer_chain->Add();
  event_url_entry->set_url(item.GetURL().spec());
  event_url_entry->set_type(ReferrerChainEntry::EVENT_URL);
  event_url_entry->set_referrer_url(
      render_frame_host->GetLastCommittedURL().spec());
  event_url_entry->set_is_retargeting(false);
  event_url_entry->set_navigation_time_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  for (const GURL& url : item.GetUrlChain()) {
    event_url_entry->add_server_redirect_chain()->set_url(url.spec());
  }
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
bool IsDownloadReportGatedByExtendedReporting(
    ClientSafeBrowsingReportRequest::ReportType report_type) {
  switch (report_type) {
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_RECOVERY:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_WARNING:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_BY_API:
      return false;
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_OPENED:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_AUTO_DELETED:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_PROFILE_CLOSED:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_WARNING_ANDROID:
      return true;
    default:
      NOTREACHED();
  }
}
#endif

}  // namespace

ClientDownloadRequestModification NoModificationToRequestProto() {
  return base::DoNothing();
}

GURL GetFileSystemAccessDownloadUrl(const GURL& frame_url) {
  // Regular blob: URLs are of the form
  // "blob:https://my-origin.com/def07373-cbd8-49d2-9ef7-20b071d34a1a". To make
  // these URLs distinguishable from those we use a fixed string rather than a
  // random UUID.
  return GURL("blob:" + frame_url.DeprecatedGetOriginAsURL().spec() +
              "file-system-access-write");
}

google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
SelectArchiveEntries(const google::protobuf::RepeatedPtrField<
                     ClientDownloadRequest::ArchivedBinary>& src_binaries) {
  // Limit the number of entries so we don't clog the backend.
  // We can expand this limit by pushing a new download_file_types update.
  size_t limit =
      FileTypePolicies::GetInstance()->GetMaxArchivedBinariesToReport();

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected;

  std::vector<ClientDownloadRequest::ArchivedBinary> considering;
  for (const ClientDownloadRequest::ArchivedBinary& entry : src_binaries) {
    if (entry.is_executable() || entry.is_archive()) {
      considering.push_back(entry);
    }
  }

  if (static_cast<size_t>(selected.size()) < limit) {
    SelectEncryptedEntry(&considering, &selected);
  }

  if (static_cast<size_t>(selected.size()) < limit) {
    SelectDeepestEntry(&considering, &selected);
  }

  std::sort(considering.begin(), considering.end(),
            [](const ClientDownloadRequest::ArchivedBinary& lhs,
               const ClientDownloadRequest::ArchivedBinary& rhs) {
              // The comparator should return true if `lhs` should come before
              // `rhs`. We want the shallowest and highest-weight entries first.
              if (ArchiveEntryDepth(lhs) != ArchiveEntryDepth(rhs)) {
                return ArchiveEntryDepth(lhs) < ArchiveEntryDepth(rhs);
              }

              return ArchiveEntryWeight(lhs) > ArchiveEntryWeight(rhs);
            });

  // Only add the wildcard if we otherwise wouldn't be able to fit all the
  // entries.
  bool should_choose_wildcard = static_cast<size_t>(selected.size()) < limit &&
                                considering.size() + selected.size() > limit;
  if (should_choose_wildcard) {
    --limit;
  }

  auto last_taken_it = considering.begin();
  for (auto binary_it = considering.begin(); binary_it != considering.end();
       ++binary_it) {
    if (static_cast<size_t>(selected.size()) >= limit) {
      break;
    }

    if (binary_it->is_executable() || binary_it->is_archive()) {
      *selected.Add() = std::move(*binary_it);
      last_taken_it = binary_it;
    }
  }

  // By actually choosing the wildcard at the end, we ensure that all the other
  // entries in the ping are completely deterministic.
  if (should_choose_wildcard && last_taken_it != considering.end()) {
    ++last_taken_it;
    considering.erase(considering.begin(), last_taken_it);
    SelectWildcardEntryAtFront(&considering, &selected);
  }

  return selected;
}

void LogDeepScanEvent(download::DownloadItem* item, DeepScanEvent event) {
  base::UmaHistogramEnumeration("SBClientDownload.DeepScanEvent3", event);
  if (DownloadItemWarningData::IsTopLevelEncryptedArchive(item)) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.PasswordProtectedDeepScanEvent3", event);
  }
}

void LogDeepScanEvent(const DeepScanningMetadata& metadata,
                      DeepScanEvent event) {
  base::UmaHistogramEnumeration("SBClientDownload.DeepScanEvent3", event);
  if (metadata.IsTopLevelEncryptedArchive()) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.PasswordProtectedDeepScanEvent3", event);
  }
}

void LogLocalDecryptionEvent(DeepScanEvent event) {
  base::UmaHistogramEnumeration("SBClientDownload.LocalDecryptionEvent", event);
}

std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
    const download::DownloadItem& item,
    int user_gesture_limit) {
  std::unique_ptr<ReferrerChain> referrer_chain =
      std::make_unique<ReferrerChain>();
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(
          const_cast<download::DownloadItem*>(&item));
  if (!web_contents) {
    return nullptr;
  }

  content::RenderFrameHost* render_frame_host =
      content::DownloadItemUtils::GetRenderFrameHost(&item);
  content::RenderFrameHost* outermost_render_frame_host =
      render_frame_host ? render_frame_host->GetOutermostMainFrame() : nullptr;
  content::GlobalRenderFrameHostId frame_id =
      outermost_render_frame_host ? outermost_render_frame_host->GetGlobalId()
                                  : content::GlobalRenderFrameHostId();

  SessionID download_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents);
  // We look for the referrer chain that leads to the download url first.
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      GetNavigationObserverManager(web_contents)
          ->IdentifyReferrerChainByEventURL(item.GetURL(), download_tab_id,
                                            frame_id, user_gesture_limit,
                                            referrer_chain.get());

  // If no navigation event is found, this download is not triggered by regular
  // navigation (e.g. html5 file apis, etc). We look for the referrer chain
  // based on relevant RenderFrameHost instead.
  if (result ==
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND &&
      web_contents && outermost_render_frame_host &&
      outermost_render_frame_host->GetLastCommittedURL().is_valid()) {
    AddEventUrlToReferrerChain(item, outermost_render_frame_host,
                               referrer_chain.get());
    result = GetNavigationObserverManager(web_contents)
                 ->IdentifyReferrerChainByRenderFrameHost(
                     outermost_render_frame_host, user_gesture_limit,
                     referrer_chain.get());
  }

  size_t referrer_chain_length = referrer_chain->size();

  // Determines how many recent navigation events to append to referrer chain
  // if any.
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  size_t recent_navigations_to_collect =
      web_contents ? SafeBrowsingNavigationObserverManager::
                         CountOfRecentNavigationsToAppend(
                             profile, profile->GetPrefs(), result)
                   : 0u;
  GetNavigationObserverManager(web_contents)
      ->AppendRecentNavigations(recent_navigations_to_collect,
                                referrer_chain.get());

  return std::make_unique<ReferrerChainData>(result, std::move(referrer_chain),
                                             referrer_chain_length,
                                             recent_navigations_to_collect);
}

std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
    const content::FileSystemAccessWriteItem& item,
    int user_gesture_limit) {
  // If web_contents is null, return immediately. This can happen when the
  // file system API is called in PerformAfterWriteChecks.
  if (!item.web_contents) {
    return nullptr;
  }

  std::unique_ptr<ReferrerChain> referrer_chain =
      std::make_unique<ReferrerChain>();

  SessionID tab_id =
      sessions::SessionTabHelper::IdForTab(item.web_contents.get());

  GURL tab_url = item.web_contents->GetVisibleURL();

  SafeBrowsingNavigationObserverManager::AttributionResult result =
      GetNavigationObserverManager(item.web_contents.get())
          ->IdentifyReferrerChainByHostingPage(
              item.frame_url, tab_url, item.outermost_main_frame_id, tab_id,
              item.has_user_gesture, user_gesture_limit, referrer_chain.get());

  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.NativeFileSystemWriteAttribution",
      result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  size_t referrer_chain_length = referrer_chain->size();

  // Determines how many recent navigation events to append to referrer chain
  // if any.
  auto* profile = Profile::FromBrowserContext(item.browser_context);
  size_t recent_navigations_to_collect =
      item.browser_context ? SafeBrowsingNavigationObserverManager::
                                 CountOfRecentNavigationsToAppend(
                                     profile, profile->GetPrefs(), result)
                           : 0u;
  GetNavigationObserverManager(item.web_contents.get())
      ->AppendRecentNavigations(recent_navigations_to_collect,
                                referrer_chain.get());

  return std::make_unique<ReferrerChainData>(result, std::move(referrer_chain),
                                             referrer_chain_length,
                                             recent_navigations_to_collect);
}

ReferrerChain GetOrIdentifyReferrerChainForEnterprise(
    download::DownloadItem& item) {
  ReferrerChain referrer_chain =
      enterprise_connectors::GetCachedReferrerChain(item);
  if (!referrer_chain.empty()) {
    return referrer_chain;
  }

  std::unique_ptr<safe_browsing::ReferrerChainData> new_referrer_chain_data =
      safe_browsing::IdentifyReferrerChain(
          item, enterprise_connectors::kReferrerUserGestureLimit);

  // If the chain can't be obtained from `safe_browsing::IdentifyReferrerChain`
  // or if the returned data only contains the download URL, fall back to
  // enterprise-specific logic to cache a value.
  if (!new_referrer_chain_data ||
      !new_referrer_chain_data->GetReferrerChain() ||
      new_referrer_chain_data->GetReferrerChain()->size() <= 1) {
    referrer_chain = enterprise_connectors::GetOrCreateReferrerChain(item);
  } else {
    referrer_chain = *new_referrer_chain_data->GetReferrerChain();
  }

  if (!referrer_chain.empty()) {
    enterprise_connectors::SetReferrerChain(referrer_chain, item);
  }

  return referrer_chain;
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
bool ShouldSendDangerousDownloadReport(
    download::DownloadItem* item,
    ClientSafeBrowsingReportRequest::ReportType report_type) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return false;
  }
  if (!IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return false;
  }
  if (IsDownloadReportGatedByExtendedReporting(report_type) &&
      !IsExtendedReportingEnabled(*profile->GetPrefs())) {
    return false;
  }
  if (browser_context->IsOffTheRecord()) {
    return false;
  }
  if (item->GetURL().is_empty() || !item->GetURL().is_valid()) {
    return false;
  }

  download::DownloadDangerType danger_type = item->GetDangerType();
  std::string token = DownloadProtectionService::GetDownloadPingToken(item);
  bool has_token = !token.empty();

  ClientDownloadResponse::Verdict download_verdict =
      safe_browsing::DownloadProtectionService::GetDownloadProtectionVerdict(
          item);
  bool has_unsafe_verdict = download_verdict != ClientDownloadResponse::SAFE;

  if (item->IsDangerous() ||
      danger_type == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    // Report downloads that are known to be dangerous or was dangerous but
    // was validated by the user.
    // DANGEROUS_URL doesn't have token or unsafe verdict since this is flagged
    // by blocklist check.
    return (has_token && has_unsafe_verdict) ||
           danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
  } else if (danger_type ==
                 download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
    // Async scanning may be triggered when the verdict is safe. Still send the
    // report in this case.
    return has_token;
  } else {
    return false;
  }
}
#endif

std::optional<enterprise_connectors::AnalysisSettings>
ShouldUploadBinaryForDeepScanning(download::DownloadItem* item) {
#if BUILDFLAG(IS_ANDROID)
  // Deep scanning is not supported on Android.
  return std::nullopt;
#else
  // Create temporary metadata wrapper on the stack.
  DownloadItemMetadata metadata(item);
  return DeepScanningRequest::ShouldUploadBinary(metadata);
#endif
}

bool IsFiletypeSupportedForFullDownloadProtection(
    const base::FilePath& file_name) {
  // On Android, do not use FileTypePolicies, which are currently only
  // applicable to desktop platforms. Instead, hardcode the APK filetype check
  // for Android here.
  // TODO(chlily): Refactor/fix FileTypePolicies and then remove this
  // platform-specific hardcoded behavior.
#if BUILDFLAG(IS_ANDROID)
  return file_name.MatchesExtension(kApkSuffix);
#else
  return FileTypePolicies::GetInstance()->IsCheckedBinaryFile(file_name);
#endif
}

}  // namespace safe_browsing

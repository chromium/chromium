// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/download_item_utils.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

namespace safe_browsing {

namespace {

// Escapes a certificate attribute so that it can be used in a allowlist
// entry.  Currently, we only escape slashes, since they are used as a
// separator between attributes.
std::string EscapeCertAttribute(const std::string& attribute) {
  std::string escaped;
  for (size_t i = 0; i < attribute.size(); ++i) {
    if (attribute[i] == '%') {
      escaped.append("%25");
    } else if (attribute[i] == '/') {
      escaped.append("%2F");
    } else {
      escaped.push_back(attribute[i]);
    }
  }
  return escaped;
}

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
  auto it = base::ranges::find_if(
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
  auto it = base::ranges::max_element(*considering, {}, &ArchiveEntryDepth);
  if (it != considering->end()) {
    *selected->Add() = *it;
    considering->erase(it);
  }
}

void SelectWildcardEntryAtFront(
    std::vector<ClientDownloadRequest::ArchivedBinary>* considering,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        selected) {
  int remaining_executables = base::ranges::count_if(
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
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
      return true;
    default:
      NOTREACHED();
  }
}
#endif

}  // namespace

void GetCertificateAllowlistStrings(
    const net::X509Certificate& certificate,
    const net::X509Certificate& issuer,
    std::vector<std::string>* allowlist_strings) {
  // The allowlist paths are in the format:
  // cert/<ascii issuer fingerprint>[/CN=common_name][/O=org][/OU=unit]
  //
  // Any of CN, O, or OU may be omitted from the allowlist entry, in which
  // case they match anything.  However, the attributes that do appear will
  // always be in the order shown above.  At least one attribute will always
  // be present.

  const net::CertPrincipal& subject = certificate.subject();
  std::vector<std::string> ou_tokens;
  for (size_t i = 0; i < subject.organization_unit_names.size(); ++i) {
    ou_tokens.push_back(
        "/OU=" + EscapeCertAttribute(subject.organization_unit_names[i]));
  }

  std::vector<std::string> o_tokens;
  for (size_t i = 0; i < subject.organization_names.size(); ++i) {
    o_tokens.push_back("/O=" +
                       EscapeCertAttribute(subject.organization_names[i]));
  }

  std::string cn_token;
  if (!subject.common_name.empty()) {
    cn_token = "/CN=" + EscapeCertAttribute(subject.common_name);
  }

  std::set<std::string> paths_to_check;
  if (!cn_token.empty()) {
    paths_to_check.insert(cn_token);
  }
  for (size_t i = 0; i < o_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + o_tokens[i]);
    paths_to_check.insert(o_tokens[i]);
    for (size_t j = 0; j < ou_tokens.size(); ++j) {
      paths_to_check.insert(cn_token + o_tokens[i] + ou_tokens[j]);
      paths_to_check.insert(o_tokens[i] + ou_tokens[j]);
    }
  }
  for (size_t i = 0; i < ou_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + ou_tokens[i]);
    paths_to_check.insert(ou_tokens[i]);
  }

  std::string issuer_fp = base::HexEncode(
      base::SHA1Hash(net::x509_util::CryptoBufferAsSpan(issuer.cert_buffer())));
  for (auto it = paths_to_check.begin(); it != paths_to_check.end(); ++it) {
    allowlist_strings->push_back("cert/" + issuer_fp + *it);
  }
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

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(item.web_contents);

  GURL tab_url = item.web_contents->GetVisibleURL();

  SafeBrowsingNavigationObserverManager::AttributionResult result =
      GetNavigationObserverManager(item.web_contents)
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
  GetNavigationObserverManager(item.web_contents)
      ->AppendRecentNavigations(recent_navigations_to_collect,
                                referrer_chain.get());

  return std::make_unique<ReferrerChainData>(result, std::move(referrer_chain),
                                             referrer_chain_length,
                                             recent_navigations_to_collect);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
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

}  // namespace safe_browsing

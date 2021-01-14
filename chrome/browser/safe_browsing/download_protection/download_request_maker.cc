// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

namespace {

DownloadRequestMaker::TabUrls TabUrlsFromWebContents(
    content::WebContents* web_contents) {
  DownloadRequestMaker::TabUrls result;
  if (web_contents) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetVisibleEntry();
    if (entry) {
      result.url = entry->GetURL();
      result.referrer = entry->GetReferrer().url;
    }
  }
  return result;
}

}  // namespace

DownloadRequestMaker::DownloadRequestMaker(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    download::DownloadItem* item)
    : browser_context_(content::DownloadItemUtils::GetBrowserContext(item)),
      request_(std::make_unique<ClientDownloadRequest>()),
      binary_feature_extractor_(binary_feature_extractor),
      tab_urls_({item->GetTabUrl(), item->GetTabReferrerUrl()}),
      target_file_path_(item->GetTargetFilePath()),
      full_path_(item->GetFullPath()) {
  request_->set_url(ShortURLForReporting(item->GetURL()));
  request_->mutable_digests()->set_sha256(item->GetHash());
  request_->set_length(item->GetReceivedBytes());
  for (size_t i = 0; i < item->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    resource->set_url(ShortURLForReporting(item->GetUrlChain()[i]));
    if (i == item->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(ShortURLForReporting(item->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource->url();
      if (!item->GetRemoteAddress().empty()) {
        resource->set_remote_ip(item->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource->referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource->url();
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
  }

  request_->set_user_initiated(item->HasUserGesture());

  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      item->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request_->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request_->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
  }
}

DownloadRequestMaker::DownloadRequestMaker(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    DownloadProtectionService* service,
    const content::FileSystemAccessWriteItem& item)
    : browser_context_(item.browser_context),
      request_(std::make_unique<ClientDownloadRequest>()),
      binary_feature_extractor_(binary_feature_extractor),
      tab_urls_(TabUrlsFromWebContents(item.web_contents)),
      target_file_path_(item.target_file_path),
      full_path_(item.full_path) {
  request_->set_url(
      ShortURLForReporting(GetFileSystemAccessDownloadUrl(item.frame_url)));
  request_->mutable_digests()->set_sha256(item.sha256_hash);
  request_->set_length(item.size);
  {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    resource->set_url(
        ShortURLForReporting(GetFileSystemAccessDownloadUrl(item.frame_url)));
    resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
    if (item.frame_url.is_valid())
      resource->set_referrer(ShortURLForReporting(item.frame_url));
  }

  request_->set_user_initiated(item.has_user_gesture);

  std::unique_ptr<ReferrerChainData> referrer_chain_data =
      service->IdentifyReferrerChain(item);
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request_->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request_->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
  }
}

DownloadRequestMaker::~DownloadRequestMaker() = default;

void DownloadRequestMaker::Start(DownloadRequestMaker::Callback callback) {
  callback_ = std::move(callback);

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  bool is_extended_reporting =
      profile && IsExtendedReportingEnabled(*profile->GetPrefs());
  bool is_incognito = browser_context_ && browser_context_->IsOffTheRecord();
  bool is_under_advanced_protection =
      profile && AdvancedProtectionStatusManagerFactory::GetForProfile(profile)
                     ->IsUnderAdvancedProtection();
  bool is_enhanced_protection =
      profile && IsEnhancedProtectionEnabled(*profile->GetPrefs());

  auto population = is_enhanced_protection
                        ? ChromeUserPopulation::ENHANCED_PROTECTION
                        : is_extended_reporting
                              ? ChromeUserPopulation::EXTENDED_REPORTING
                              : ChromeUserPopulation::SAFE_BROWSING;
  request_->mutable_population()->set_user_population(population);
  request_->mutable_population()->set_profile_management_status(
      GetProfileManagementStatus(
          g_browser_process->browser_policy_connector()));
  request_->mutable_population()->set_is_under_advanced_protection(
      is_under_advanced_protection);
  request_->mutable_population()->set_is_incognito(is_incognito);
  request_->set_request_ap_verdicts(is_under_advanced_protection);
  request_->set_locale(g_browser_process->GetApplicationLocale());
  request_->set_file_basename(target_file_path_.BaseName().AsUTF8Unsafe());

  file_analyzer_->Start(
      target_file_path_, full_path_,
      base::BindOnce(&DownloadRequestMaker::OnFileFeatureExtractionDone,
                     weakptr_factory_.GetWeakPtr()));
}

void DownloadRequestMaker::OnFileFeatureExtractionDone(
    FileAnalyzer::Results results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  request_->set_download_type(results.type);
  if (results.archive_is_valid != FileAnalyzer::ArchiveValid::UNSET)
    request_->set_archive_valid(results.archive_is_valid ==
                                FileAnalyzer::ArchiveValid::VALID);
  request_->mutable_archived_binary()->CopyFrom(results.archived_binaries);
  request_->mutable_signature()->CopyFrom(results.signature_info);
  request_->mutable_image_headers()->CopyFrom(results.image_headers);
  request_->set_archive_file_count(results.file_count);
  request_->set_archive_directory_count(results.directory_count);

#if defined(OS_MAC)
  if (!results.disk_image_signature.empty()) {
    request_->set_udif_code_signature(results.disk_image_signature.data(),
                                      results.disk_image_signature.size());
  }
  if (!results.detached_code_signatures.empty()) {
    request_->mutable_detached_code_signature()->CopyFrom(
        results.detached_code_signatures);
  }
#endif

  GetTabRedirects();
}

void DownloadRequestMaker::GetTabRedirects() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_urls_.url.is_valid()) {
    OnGotTabRedirects({});
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history) {
    OnGotTabRedirects({});
    return;
  }

  history->QueryRedirectsTo(
      tab_urls_.url,
      base::BindOnce(&DownloadRequestMaker::OnGotTabRedirects,
                     weakptr_factory_.GetWeakPtr()),
      &request_tracker_);
}

void DownloadRequestMaker::OnGotTabRedirects(
    history::RedirectList redirect_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (size_t i = 0; i < redirect_list.size(); ++i) {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    DVLOG(2) << "tab redirect " << i << " " << redirect_list[i].spec();
    resource->set_url(ShortURLForReporting(redirect_list[i]));
    resource->set_type(ClientDownloadRequest::TAB_REDIRECT);
  }
  if (tab_urls_.url.is_valid()) {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    resource->set_url(ShortURLForReporting(tab_urls_.url));
    DVLOG(2) << "tab url " << resource->url();
    resource->set_type(ClientDownloadRequest::TAB_URL);
    if (tab_urls_.referrer.is_valid()) {
      resource->set_referrer(ShortURLForReporting(tab_urls_.referrer));
      DVLOG(2) << "tab referrer " << resource->referrer();
    }
  }

  std::move(callback_).Run(std::move(request_));
}

}  // namespace safe_browsing

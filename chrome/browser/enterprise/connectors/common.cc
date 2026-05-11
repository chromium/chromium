// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/connectors/core/features.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"

using safe_browsing::CloudBinaryUploadServiceFactory;
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
// URL chain limit for nested iFrames.
constexpr int kMaxFrameUrls = 10;

google::protobuf::RepeatedPtrField<std::string> CollectFrameUrlsImpl(
    content::WebContents* web_contents) {
  google::protobuf::RepeatedPtrField<std::string> frame_urls;

  if (!web_contents) {
    return frame_urls;
  }

  content::RenderFrameHost* current_frame = web_contents->GetFocusedFrame();

  // Traverse upwards and add URLs to the chain, stopping before the outermost
  // frame.
  while (current_frame && frame_urls.size() < kMaxFrameUrls) {
    content::RenderFrameHost* parent =
        current_frame->GetParentOrOuterDocumentOrEmbedder();
    if (!parent) {
      // Already at outermost frame.
      break;
    }

    // Skip internal extension resources, blob URLs, and about:blank pages from
    // being scanned.
    const GURL& url = current_frame->GetLastCommittedURL();
    bool should_skip =
        url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kBlobScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    should_skip |= url.SchemeIs(extensions::kExtensionScheme);
#endif
    if (!should_skip) {
      *frame_urls.Add() = url.spec();
    }

    current_frame = parent;
  }

  return frame_urls;
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

policy::BrowserPolicyConnector* GetBrowserPolicyConnector() {
  return g_browser_process ? g_browser_process->browser_policy_connector()
                           : nullptr;
}

const char SavePackageScanningData::kKey[] =
    "enterprise_connectors.save_package_scanning_key";
SavePackageScanningData::SavePackageScanningData(
    content::SavePackageAllowedCallback callback)
    : callback(std::move(callback)) {}
SavePackageScanningData::~SavePackageScanningData() = default;

void RunSavePackageScanningCallback(download::DownloadItem* item,
                                    bool allowed) {
  DCHECK(item);

  auto* data = static_cast<SavePackageScanningData*>(
      item->GetUserData(SavePackageScanningData::kKey));
  if (data && !data->callback.is_null())
    std::move(data->callback).Run(allowed);
}

bool IncludeDeviceInfo(Profile* profile, bool per_profile) {
#if BUILDFLAG(IS_CHROMEOS)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
#else
  // A browser managed through the device can send device info.
  if (!per_profile) {
    return true;
  }

  // An unmanaged browser shouldn't share its device info for privacy reasons.
  if (!policy::GetDMToken(profile).is_valid()) {
    return false;
  }

  // A managed device can share its info with the profile if they are
  // affiliated.
  return enterprise_util::IsProfileAffiliated(profile);
#endif
}

std::string GetProfileEmail(Profile* profile) {
  if (!profile) {
    return std::string();
  }

  std::string email =
      GetProfileEmail(IdentityManagerFactory::GetForProfile(profile));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (email.empty()) {
    email = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return email;
}

google::protobuf::RepeatedPtrField<std::string> CollectFrameUrls(
    content::WebContents* web_contents,
    DeepScanAccessPoint access_point) {
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  if (!base::FeatureList::IsEnabled(kEnterpriseIframeDlpRulesSupport)) {
    return google::protobuf::RepeatedPtrField<std::string>();
  }

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrlsImpl(web_contents);

  // For the histogram, we count the tab URL to differentiate between cases
  // where there is no tab and tabs with no iframes.
  size_t full_chain_size = web_contents ? frame_urls.size() + 1 : 0;
  base::UmaHistogramCustomCounts(
      base::JoinString(
          {"Enterprise.IframeDlpRulesSupport",
           DeepScanAccessPointToString(access_point), "UrlChainSize"},
          "."),
      full_chain_size, 1, kMaxFrameUrls, 10);

  return frame_urls;
#else
  return google::protobuf::RepeatedPtrField<std::string>();
#endif
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)

BinaryUploadService* GetBinaryUploadServiceForConnector(
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& settings) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (settings.cloud_or_local_settings.is_cloud_analysis()) {
    return CloudBinaryUploadServiceFactory::GetForProfile(profile);
  } else {
    return LocalBinaryUploadServiceFactory::GetForProfile(profile);
  }
#else
  DCHECK(settings.cloud_or_local_settings.is_cloud_analysis());
  return CloudBinaryUploadServiceFactory::GetForProfile(profile);
#endif
}

#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
bool ShouldPromptReviewForDownload(
    Profile* profile,
    const download::DownloadItem* download_item) {
  // Review dialog only appears if custom UI has been set by the admin or custom
  // rule message present in download item.
  if (!download_item) {
    return false;
  }
  download::DownloadDangerType danger_type = download_item->GetDangerType();
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
      danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
               ->HasExtraUiToDisplay(AnalysisConnector::FILE_DOWNLOADED,
                                     kDlpTag) ||
           GetDownloadsCustomRuleMessage(download_item, danger_type);
  } else if (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
        ->HasExtraUiToDisplay(AnalysisConnector::FILE_DOWNLOADED, kMalwareTag);
  }
  return false;
}

void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure) {
  auto state = FinalContentAnalysisResult::FAILURE;
  download::DownloadDangerType danger_type = download_item->GetDangerType();

  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
    state = FinalContentAnalysisResult::WARNING;
  }

  const char* tag =
      (danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
               danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK
           ? kDlpTag
           : kMalwareTag);

  auto* connectors_service =
      ConnectorsServiceFactory::GetForBrowserContext(profile);

  std::u16string custom_message =
      connectors_service
          ->GetCustomMessage(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(u"");
  GURL learn_more_url =
      connectors_service
          ->GetLearnMoreUrl(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(GURL());

  bool bypass_justification_required =
      connectors_service->GetBypassJustificationRequired(
          AnalysisConnector::FILE_DOWNLOADED, tag);

  // This dialog opens itself, and is thereafter owned by constrained window
  // code.
  new ContentAnalysisDialogController(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          filename, custom_message, learn_more_url,
          bypass_justification_required, std::move(keep_closure),
          std::move(discard_closure), download_item,
          GetDownloadsCustomRuleMessage(download_item, danger_type)
              .value_or(ContentAnalysisResponse::Result::TriggeredRule::
                            CustomRuleMessage())),
      true,  // Downloads are always cloud-based for now.
      web_contents, DeepScanAccessPoint::DOWNLOAD,
      /* file_count */ 1, state, download_item);
}

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace enterprise_connectors

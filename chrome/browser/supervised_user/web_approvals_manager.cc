// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/web_approvals_manager.h"
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#endif

namespace {

constexpr char kLocalWebApprovalDurationHistogramName[] =
    "FamilyLinkUser.LocalWebApprovalCompleteRequestTotalDuration";

void CreateURLAccessRequest(
    const GURL& url,
    PermissionRequestCreator* creator,
    WebApprovalsManager::ApprovalRequestInitiatedCallback callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

// Helper method for getting human readable outcome for a local web approval.
std::string EnumLocalWebApprovalFlowOutcomeToString(
    AndroidLocalWebApprovalFlowOutcome outcome) {
  switch (outcome) {
    case AndroidLocalWebApprovalFlowOutcome::kApproved:
      return "Approved";
    case AndroidLocalWebApprovalFlowOutcome::kRejected:
      return "Rejected";
    case AndroidLocalWebApprovalFlowOutcome::kIncomplete:
      return "Incomplete";
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Helper method for getting human readable outcome for a local web approval.
std::string ParentAccessResultToLoggingStringChromeOS(
    ash::ParentAccessDialog::Result::Status outcome) {
  switch (outcome) {
    case ash::ParentAccessDialog::Result::Status::kApproved:
      return "Approved";
    case ash::ParentAccessDialog::Result::Status::kDeclined:
      return "Declined";
    case ash::ParentAccessDialog::Result::Status::kCancelled:
      return "Cancelled";
    case ash::ParentAccessDialog::Result::Status::kError:
      return "Error";
  }
}
#endif

void RecordTimeToApprovalDurationMetric(base::TimeDelta durationMs) {
  base::UmaHistogramLongTimes(kLocalWebApprovalDurationHistogramName,
                              durationMs);
}

}  // namespace

WebApprovalsManager::WebApprovalsManager() = default;

WebApprovalsManager::~WebApprovalsManager() = default;

void WebApprovalsManager::RequestLocalApproval(
    content::WebContents* web_contents,
    const GURL& url,
    const std::u16string& child_display_name,
    const gfx::ImageSkia& favicon,
    ApprovalRequestInitiatedCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/250954669): replace this with call to the ParentAccess crosapi with
  // appropriate parameters and handle the ParentAccess crosapi result.
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetProfileKey());
  std::vector<uint8_t> favicon_bytes;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*favicon.bitmap(), false,
                                        &favicon_bytes);
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New(
                  url.GetWithEmptyPath(), child_display_name, favicon_bytes)));

  ash::ParentAccessDialogProvider provider;
  ash::ParentAccessDialogProvider::ShowError result = provider.Show(
      std::move(params),
      base::BindOnce(
          &WebApprovalsManager::OnLocalApprovalRequestCompletedChromeOS,
          weak_ptr_factory_.GetWeakPtr(), settings_service, url,
          base::TimeTicks::Now()));

  if (result != ash::ParentAccessDialogProvider::ShowError::kNone) {
    LOG(ERROR) << "Error showing ParentAccessDialog: "
               << static_cast<int>(result);
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
#elif BUILDFLAG(IS_ANDROID)
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetProfileKey());
  WebsiteParentApproval::RequestLocalApproval(
      web_contents, NormalizeUrl(url),
      base::BindOnce(&WebApprovalsManager::OnLocalApprovalRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), settings_service, url,
                     base::TimeTicks::Now()));
  std::move(callback).Run(true);
#endif
}

void WebApprovalsManager::RequestRemoteApproval(
    const GURL& url,
    ApprovalRequestInitiatedCallback callback) {
  AddRemoteApprovalRequestInternal(
      base::BindRepeating(CreateURLAccessRequest, NormalizeUrl(url)),
      std::move(callback), 0);
}

bool WebApprovalsManager::AreRemoteApprovalRequestsEnabled() const {
  return FindEnabledRemoteApprovalRequestCreator(0) <
         remote_approval_request_creators_.size();
}

void WebApprovalsManager::AddRemoteApprovalRequestCreator(
    std::unique_ptr<PermissionRequestCreator> creator) {
  remote_approval_request_creators_.push_back(std::move(creator));
}

void WebApprovalsManager::ClearRemoteApprovalRequestsCreators() {
  remote_approval_request_creators_.clear();
}

size_t WebApprovalsManager::FindEnabledRemoteApprovalRequestCreator(
    size_t start) const {
  for (size_t i = start; i < remote_approval_request_creators_.size(); ++i) {
    if (remote_approval_request_creators_[i]->IsEnabled())
      return i;
  }
  return remote_approval_request_creators_.size();
}

GURL WebApprovalsManager::NormalizeUrl(const GURL& url) {
  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;
  return url_matcher::util::Normalize(effective_url);
}

void WebApprovalsManager::AddRemoteApprovalRequestInternal(
    const CreateRemoteApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index) {
  size_t next_index = FindEnabledRemoteApprovalRequestCreator(index);
  if (next_index >= remote_approval_request_creators_.size()) {
    std::move(callback).Run(false);
    return;
  }

  create_request.Run(
      remote_approval_request_creators_[next_index].get(),
      base::BindOnce(&WebApprovalsManager::OnRemoteApprovalRequestIssued,
                     weak_ptr_factory_.GetWeakPtr(), create_request,
                     std::move(callback), next_index));
}

void WebApprovalsManager::OnRemoteApprovalRequestIssued(
    const CreateRemoteApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  AddRemoteApprovalRequestInternal(create_request, std::move(callback),
                                   index + 1);
}

void WebApprovalsManager::OnLocalApprovalRequestCompleted(
    SupervisedUserSettingsService* settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    AndroidLocalWebApprovalFlowOutcome request_outcome) {
  VLOG(0) << "Local URL approval final result: "
          << EnumLocalWebApprovalFlowOutcomeToString(request_outcome);

  // Record duration metrics only for completed approval flows.
  if (request_outcome == AndroidLocalWebApprovalFlowOutcome::kApproved ||
      request_outcome == AndroidLocalWebApprovalFlowOutcome::kRejected) {
    RecordTimeToApprovalDurationMetric(base::TimeTicks::Now() - start_time);
  }

  if (request_outcome == AndroidLocalWebApprovalFlowOutcome::kApproved) {
    settings_service->RecordLocalWebsiteApproval(url.host());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebApprovalsManager::OnLocalApprovalRequestCompletedChromeOS(
    SupervisedUserSettingsService* settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  VLOG(0) << "Local URL approval final result: "
          << ParentAccessResultToLoggingStringChromeOS(result->status);

  // Record duration metrics only for completed approval flows.
  if (result->status == ash::ParentAccessDialog::Result::Status::kApproved ||
      result->status == ash::ParentAccessDialog::Result::Status::kDeclined) {
    RecordTimeToApprovalDurationMetric(base::TimeTicks::Now() - start_time);
  }

  // TODO(b/250947827): Add request result metric for CrOS.
  if (result->status == ash::ParentAccessDialog::Result::Status::kApproved) {
    settings_service->RecordLocalWebsiteApproval(url.host());
  }
}
#endif

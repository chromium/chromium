// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_flow.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"
#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"
#include "chrome/browser/save_to_drive/save_to_drive_event_dispatcher.h"
#include "chrome/browser/save_to_drive/save_to_drive_utils.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/save_to_drive/get_account.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/pdf_features.h"

namespace save_to_drive {
namespace {

using content::RenderFrameHost;
using content::WebContents;
using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

constexpr base::TimeDelta kHatsSurveyTimeout = base::Seconds(4);
constexpr base::ByteCount kMultipartUploadThreshold = base::MiB(5);

WebContents* GetTabWebContents(RenderFrameHost* render_frame_host) {
  auto stream = GetStreamWeakPtr(render_frame_host);
  WebContents* web_contents = nullptr;
  if (stream) {
    extensions::ExtensionTabUtil::GetTabById(
        stream->tab_id(), render_frame_host->GetBrowserContext(),
        /*include_incognito=*/false, &web_contents);
  }
  return web_contents;
}

SaveToDriveFlow::CreateCallback* g_create_callback_for_testing = nullptr;

}  // namespace

SaveToDriveFlow::SaveToDriveFlow(
    RenderFrameHost* render_frame_host,
    std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
    std::unique_ptr<ContentReader> content_reader,
    std::unique_ptr<AccountChooser> account_chooser,
    HatsService* hats_service)
    : content::DocumentUserData<SaveToDriveFlow>(render_frame_host),
      event_dispatcher_(std::move(event_dispatcher)),
      content_reader_(std::move(content_reader)),
      account_chooser_(std::move(account_chooser)),
      hats_service_(hats_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

SaveToDriveFlow::~SaveToDriveFlow() {
  hats_service_ = nullptr;
}

// static
SaveToDriveFlow* SaveToDriveFlow::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<SaveToDriveEventDispatcher> event_dispatcher,
    std::unique_ptr<ContentReader> content_reader,
    std::unique_ptr<AccountChooser> account_chooser,
    HatsService* hats_service) {
  if (g_create_callback_for_testing) {
    return g_create_callback_for_testing->Run(
        render_frame_host, std::move(event_dispatcher),
        std::move(content_reader), std::move(account_chooser), hats_service);
  }

  SaveToDriveFlow::CreateForCurrentDocument(
      render_frame_host, std::move(event_dispatcher), std::move(content_reader),
      std::move(account_chooser), hats_service);
  return SaveToDriveFlow::GetForCurrentDocument(render_frame_host);
}

// static
void SaveToDriveFlow::SetCreateCallbackForTesting(CreateCallback* callback) {
  CHECK_IS_TEST();
  g_create_callback_for_testing = callback;
}

void SaveToDriveFlow::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kInitiated;
  progress.error_type = SaveToDriveErrorType::kNoError;
  OnUploadProgress(std::move(progress));

  WebContents* contents = GetTabWebContents(&render_frame_host());
  CHECK(contents);
  account_chooser_->GetAccount(contents,
                               base::BindOnce(&SaveToDriveFlow::OnAccountChosen,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void SaveToDriveFlow::OnAccountChosen(std::optional<AccountInfo> account_info) {
  SaveToDriveProgress progress;
  if (!account_info) {
    progress.status = SaveToDriveStatus::kUploadFailed;
    progress.error_type = SaveToDriveErrorType::kAccountChooserCanceled;
    OnUploadProgress(std::move(progress));
    return;
  }
  save_to_drive_account_info_ = {
      .email = account_info->email,
      .is_managed = account_info->IsManaged() == signin::Tribool::kTrue,
  };
  progress.status = SaveToDriveStatus::kAccountSelected;
  progress.error_type = SaveToDriveErrorType::kNoError;
  OnUploadProgress(std::move(progress));

  auto open_content_callback = base::BindOnce(&SaveToDriveFlow::OnOpenContent,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              std::move(account_info.value()));
  content_reader_->Open(std::move(open_content_callback));
}

void SaveToDriveFlow::OnOpenContent(AccountInfo account_info, bool success) {
  if (!success) {
    SaveToDriveProgress progress;
    progress.status = SaveToDriveStatus::kUploadFailed;
    progress.error_type = SaveToDriveErrorType::kUnknownError;
    OnUploadProgress(std::move(progress));
    return;
  }
  auto* web_contents = WebContents::FromRenderFrameHost(&render_frame_host());
  std::string title = base::UTF16ToUTF8(web_contents->GetTitle());

  auto upload_progress_callback = base::BindRepeating(
      &SaveToDriveFlow::OnUploadProgress, weak_ptr_factory_.GetWeakPtr());
  auto* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());

  if (base::ByteCount(content_reader_->GetSize()) < kMultipartUploadThreshold) {
    drive_uploader_ = std::make_unique<MultipartDriveUploader>(
        std::move(title), std::move(account_info),
        std::move(upload_progress_callback), profile, content_reader_.get());
  } else {
    drive_uploader_ = std::make_unique<ResumableDriveUploader>(
        std::move(title), std::move(account_info),
        std::move(upload_progress_callback), profile, content_reader_.get());
  }
  drive_uploader_->Start();
}

void SaveToDriveFlow::OnUploadProgress(SaveToDriveProgress progress) {
  bool should_stop = progress.status == SaveToDriveStatus::kUploadCompleted ||
                     progress.status == SaveToDriveStatus::kUploadFailed;
  upload_progress_ = progress.Clone();
  if (save_to_drive_account_info_) {
    progress.account_email = save_to_drive_account_info_->email;
    progress.account_is_managed = save_to_drive_account_info_->is_managed;
  }
  event_dispatcher_->Notify(std::move(progress));
  if (should_stop) {
    Stop();
  }
}

void SaveToDriveFlow::ShowHatsSurveyWithDelay() {
  SurveyBitsData product_specific_bits_data = {
      {"Upload status",
       upload_progress_ &&
           upload_progress_->status == SaveToDriveStatus::kUploadCompleted},
      {"Multipart upload",
       drive_uploader_ && drive_uploader_->get_drive_uploader_type() ==
                              DriveUploaderType::kMultipart},
      {"Resumable upload",
       drive_uploader_ && drive_uploader_->get_drive_uploader_type() ==
                              DriveUploaderType::kResumable}};
  hats_service_->LaunchDelayedSurvey(GetHatsSurveyTriggerId(),
                                     kHatsSurveyTimeout.InMilliseconds(),
                                     std::move(product_specific_bits_data));
}

std::string SaveToDriveFlow::GetHatsSurveyTriggerId() {
  if (save_to_drive_account_info_ && save_to_drive_account_info_->is_managed) {
    return kHatsSurveyEnterpriseTriggerPdfSaveToDrive;
  }
  return kHatsSurveyConsumerTriggerPdfSaveToDrive;
}

void SaveToDriveFlow::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (base::FeatureList::IsEnabled(
          chrome_pdf::features::kPdfSaveToDriveSurvey)) {
    ShowHatsSurveyWithDelay();
  }
  DeleteForCurrentDocument(&render_frame_host());
  // Don't do anything else here. The flow will be destroyed after this line.
}

SaveToDriveFlow::TestApi::TestApi(SaveToDriveFlow* flow)
    : flow_(flow->weak_ptr_factory_.GetWeakPtr()) {}

SaveToDriveFlow::TestApi::~TestApi() = default;

const ContentReader* SaveToDriveFlow::TestApi::content_reader() const {
  return flow_ ? flow_->content_reader_.get() : nullptr;
}

const DriveUploader* SaveToDriveFlow::TestApi::drive_uploader() const {
  return flow_ ? flow_->drive_uploader_.get() : nullptr;
}

const SaveToDriveEventDispatcher* SaveToDriveFlow::TestApi::event_dispatcher()
    const {
  return flow_ ? flow_->event_dispatcher_.get() : nullptr;
}

RenderFrameHost* SaveToDriveFlow::TestApi::rfh() {
  return flow_ ? &flow_->render_frame_host() : nullptr;
}

DOCUMENT_USER_DATA_KEY_IMPL(SaveToDriveFlow);

}  // namespace save_to_drive

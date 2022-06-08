// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/mojom/base/safe_base_name.mojom.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::ash::os_feedback_ui::mojom::AttachedFilePtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;
using extensions::FeedbackParams;
using extensions::FeedbackPrivateAPI;

feedback::FeedbackUploader* GetFeedbackUploaderForContext(
    content::BrowserContext* context) {
  return feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context);
}

scoped_refptr<base::RefCountedMemory> GetScreenshotData() {
  auto* screenshot_manager = OsFeedbackScreenshotManager::GetIfExists();
  if (screenshot_manager) {
    return screenshot_manager->GetScreenshotData();
  }
  return nullptr;
}

constexpr std::size_t MAX_ATTACHED_FILE_SIZE_BYTES = 10 * 1024 * 1024;

bool ShouldAddAttachment(const AttachedFilePtr& attached_file) {
  if (!(attached_file && attached_file->file_data.data())) {
    // Does not have data.
    return false;
  }
  if (attached_file->file_name.path().empty()) {
    // The file name is empty.
    return false;
  }
  if (attached_file->file_data.size() > MAX_ATTACHED_FILE_SIZE_BYTES) {
    LOG(WARNING) << "Can't upload file larger than 10 MB. File size: "
                 << attached_file->file_data.size();
    return false;
  }
  return true;
}

}  // namespace

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(Profile* profile)
    : ChromeOsFeedbackDelegate(profile,
                               FeedbackPrivateAPI::GetFactoryInstance()
                                   ->Get(profile)
                                   ->GetService()) {}

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(
    Profile* profile,
    scoped_refptr<extensions::FeedbackService> feedback_service)
    : profile_(profile), feedback_service_(feedback_service) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser) {
    // Save the last active page url before opening the feedback tool.
    page_url_ = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
}

ChromeOsFeedbackDelegate::~ChromeOsFeedbackDelegate() {
  auto* screenshot_manager = OsFeedbackScreenshotManager::GetIfExists();
  if (screenshot_manager) {
    screenshot_manager->DeleteScreenshotData();
  }
}

std::string ChromeOsFeedbackDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

absl::optional<GURL> ChromeOsFeedbackDelegate::GetLastActivePageUrl() {
  return page_url_;
}

absl::optional<std::string> ChromeOsFeedbackDelegate::GetSignedInUserEmail()
    const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager)
    return absl::nullopt;
  // Browser sync consent is not required to use feedback.
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

void ChromeOsFeedbackDelegate::GetScreenshotPng(
    GetScreenshotPngCallback callback) {
  scoped_refptr<base::RefCountedMemory> png_data = GetScreenshotData();
  if (png_data && png_data.get()) {
    std::vector<uint8_t> data(png_data->data(),
                              png_data->data() + png_data->size());
    std::move(callback).Run(data);
  } else {
    std::vector<uint8_t> empty_data;
    std::move(callback).Run(empty_data);
  }
}

void ChromeOsFeedbackDelegate::SendReport(
    os_feedback_ui::mojom::ReportPtr report,
    SendReportCallback callback) {
  // Populate feedback_params
  FeedbackParams feedback_params;
  feedback_params.form_submit_time = base::TimeTicks::Now();
  feedback_params.load_system_info = report->include_system_logs_and_histograms;
  feedback_params.send_histograms = report->include_system_logs_and_histograms;

  base::WeakPtr<feedback::FeedbackUploader> uploader =
      base::AsWeakPtr(GetFeedbackUploaderForContext(profile_));
  scoped_refptr<::feedback::FeedbackData> feedback_data =
      base::MakeRefCounted<feedback::FeedbackData>(
          std::move(uploader), ContentTracingManager::Get());

  feedback_data->set_description(base::UTF16ToUTF8(report->description));

  const auto& feedback_context = report->feedback_context;
  if (feedback_context->email.has_value()) {
    feedback_data->set_user_email(feedback_context->email.value());
  }
  if (feedback_context->page_url.has_value()) {
    feedback_data->set_page_url(feedback_context->page_url.value().spec());
  }

  scoped_refptr<base::RefCountedMemory> png_data = GetScreenshotData();
  if (report->include_screenshot && png_data && png_data.get()) {
    feedback_data->set_image(
        std::string(png_data->front_as<char>(), png_data->size()));
  }

  const AttachedFilePtr& attached_file = report->attached_file;
  if (ShouldAddAttachment(attached_file)) {
    feedback_data->set_attached_filename(
        attached_file->file_name.path().AsUTF8Unsafe());
    const std::string file_data(
        reinterpret_cast<const char*>(attached_file->file_data.data()),
        attached_file->file_data.size());
    // Compress attached file and add to |feedback_data|. The operation is done
    // by posting a task to thread pool. The |feedback_data| will manage waiting
    // for all tasks to complete.
    feedback_data->AttachAndCompressFileData(std::move(file_data));
  }

  feedback_service_->SendFeedback(
      feedback_params, feedback_data,
      base::BindOnce(&ChromeOsFeedbackDelegate::OnSendFeedbackDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOsFeedbackDelegate::OnSendFeedbackDone(SendReportCallback callback,
                                                  bool status) {
  // When status is true, it means the report will be sent shortly.
  const SendReportStatus send_status =
      status ? SendReportStatus::kSuccess : SendReportStatus::kDelayed;
  std::move(callback).Run(send_status);
}

void ChromeOsFeedbackDelegate::OpenDiagnosticsApp() {
  web_app::LaunchSystemWebAppAsync(profile_,
                                   ash::SystemWebAppType::DIAGNOSTICS);
}

}  // namespace ash

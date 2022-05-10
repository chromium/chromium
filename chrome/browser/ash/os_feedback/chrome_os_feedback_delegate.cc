// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace ash {

namespace {

feedback::FeedbackUploader* GetFeedbackUploaderForContext(
    content::BrowserContext* context) {
  return feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context);
}

void TakeScreenshot(
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)> callback) {
  aura::Window* primary_window = ash::Shell::GetPrimaryRootWindow();
  if (primary_window) {
    gfx::Rect rect = primary_window->bounds();
    ui::GrabWindowSnapshotAsyncPNG(primary_window, rect, std::move(callback));
  }
}

}  // namespace

using ::ash::os_feedback_ui::mojom::SendReportStatus;
using extensions::FeedbackParams;
using extensions::FeedbackPrivateAPI;

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(Profile* profile)
    : ChromeOsFeedbackDelegate(profile,
                               FeedbackPrivateAPI::GetFactoryInstance()
                                   ->Get(profile)
                                   ->GetService()) {}

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(
    Profile* profile,
    scoped_refptr<extensions::FeedbackService> feedback_service)
    : profile_(profile), feedback_service_(feedback_service) {
  // TODO(xiangdongkong): Take screenshot first, then open the feedback app.
  TakeScreenshot(base::BindOnce(&ChromeOsFeedbackDelegate::OnScreenshotTaken,
                                weak_ptr_factory_.GetWeakPtr()));

  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser) {
    // Save the last active page url before opening the feedback tool.
    page_url_ = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
}

ChromeOsFeedbackDelegate::~ChromeOsFeedbackDelegate() = default;

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
  if (screenshot_png_data_ && screenshot_png_data_.get()) {
    std::vector<uint8_t> data(
        screenshot_png_data_->data(),
        screenshot_png_data_->data() + screenshot_png_data_->size());
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

  if (report->include_screenshot && screenshot_png_data_ &&
      screenshot_png_data_.get()) {
    feedback_data->set_image(std::string(screenshot_png_data_->front_as<char>(),
                                         screenshot_png_data_->size()));
  }

  feedback_service_->SendFeedback(
      feedback_params, feedback_data,
      base::BindOnce(&ChromeOsFeedbackDelegate::OnSendFeedbackDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChromeOsFeedbackDelegate::OnSendFeedbackDone(SendReportCallback callback,
                                                  bool status) {
  const SendReportStatus send_status =
      status ? SendReportStatus::kDelayed : SendReportStatus::kSuccess;
  std::move(callback).Run(send_status);
}

void ChromeOsFeedbackDelegate::OnScreenshotTaken(
    scoped_refptr<base::RefCountedMemory> data) {
  if (data && data.get()) {
    screenshot_png_data_ = std::move(data);
  } else {
    LOG(ERROR) << "failed to take screenshot.";
  }
}

}  // namespace ash

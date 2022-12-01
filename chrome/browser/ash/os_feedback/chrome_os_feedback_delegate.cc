// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
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
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/feedback/child_web_dialog.h"
#include "chrome/common/webui_url_constants.h"
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

// Key-value pair to be added to FeedbackData when user grants consent to Google
// to follow-up on feedback report. See (go/feedback-user-consent-faq) for more
// information.
// Consent key matches cross-platform key.
constexpr char kFeedbackUserConsentKey[] = "feedbackUserCtlConsent";
// Consent value matches JavaScript: `String(true)`.
constexpr char kFeedbackUserConsentGrantedValue[] = "true";
// Consent value matches JavaScript: `String(false)`.
constexpr char kFeedbackUserConsentDeniedValue[] = "false";
constexpr char kExtraDiagnosticsKey[] = "EXTRA_DIAGNOSTICS";

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
  // GetLastActivePageUrl will be called when the UI is about to be displayed.
  PreloadSystemLogs();
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

int ChromeOsFeedbackDelegate::GetPerformanceTraceId() {
  if (ContentTracingManager* manager = ContentTracingManager::Get()) {
    return manager->RequestTrace();
  } else {
    return 0;
  }
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
  feedback_params.send_bluetooth_logs = report->send_bluetooth_logs;
  feedback_params.send_tab_titles = report->include_screenshot;
  feedback_params.is_internal_email =
      report->feedback_context->is_internal_account;

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
  if (feedback_context->extra_diagnostics.has_value() &&
      !feedback_context->extra_diagnostics.value().empty()) {
    feedback_data->AddLog(kExtraDiagnosticsKey,
                          feedback_context->extra_diagnostics.value());
  }
  feedback_data->set_trace_id(report->feedback_context->trace_id);
  feedback_data->set_from_assistant(feedback_context->from_assistant);
  feedback_data->set_assistant_debug_info_allowed(
      feedback_context->assistant_debug_info_allowed);

  if (feedback_context->category_tag.has_value()) {
    feedback_data->set_category_tag(feedback_context->category_tag.value());
  }

  scoped_refptr<base::RefCountedMemory> png_data = GetScreenshotData();
  if (report->include_screenshot && png_data && png_data.get()) {
    feedback_data->set_image(
        std::string(png_data->front_as<char>(), png_data->size()));
  }

  // Append consent value to report. For cross platform implementations see:
  // extensions/browser/api/feedback_private/feedback_private_api.cc
  if (report->contact_user_consent_granted) {
    feedback_data->AddLog(kFeedbackUserConsentKey,
                          kFeedbackUserConsentGrantedValue);
    ash::os_feedback_ui::metrics::EmitFeedbackAppCanContactUser(
        os_feedback_ui::metrics::FeedbackAppContactUserConsentType::kYes);
  } else {
    feedback_data->AddLog(kFeedbackUserConsentKey,
                          kFeedbackUserConsentDeniedValue);
    feedback_context->email.has_value()
        ? ash::os_feedback_ui::metrics::EmitFeedbackAppCanContactUser(
              os_feedback_ui::metrics::FeedbackAppContactUserConsentType::kNo)
        : ash::os_feedback_ui::metrics::EmitFeedbackAppCanContactUser(
              os_feedback_ui::metrics::FeedbackAppContactUserConsentType::
                  kNoEmail);
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
    // Records whether the file is included when the feedback report is
    // submitted.
    ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedFile(true);
  } else {
    ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedFile(false);
  }

  // Handle Feedback Metrics
  // Records whether the screenshot is included when the feedback report is
  // submitted.
  ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedScreenshot(
      report->include_screenshot);
  // Records whether the email is included when the feedback report is
  // submitted.
  ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedEmail(
      feedback_context->email.has_value());
  // Records whether the page url is included when the feedback report is
  // submitted.
  ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedUrl(
      feedback_context->page_url.has_value());
  // Records whether the system and information is included when the feedback
  // report is submitted.
  ash::os_feedback_ui::metrics::EmitFeedbackAppIncludedSystemInfo(
      report->include_system_logs_and_histograms);
  // Records the length of description in the textbox when the feedback
  // report is submitted.
  ash::os_feedback_ui::metrics::EmitFeedbackAppDescriptionLength(
      report->description.length());

  // If system logs are included, get them from preloaded response.
  if (feedback_params.load_system_info && system_logs_response_ &&
      system_logs_response_->size() > 0) {
    for (auto& itr : *system_logs_response_) {
      if (FeedbackCommon::IncludeInSystemLogs(
              itr.first, feedback_params.is_internal_email))
        feedback_data->AddLog(std::move(itr.first), std::move(itr.second));
    }
    // Set to false so they won't be loaded again in feedback service.
    feedback_params.load_system_info = false;
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
  ash::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::DIAGNOSTICS);
}

void ChromeOsFeedbackDelegate::OpenExploreApp() {
  ash::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::HELP);
}

void ChromeOsFeedbackDelegate::OpenMetricsDialog() {
  OpenWebDialog(GURL(chrome::kChromeUIHistogramsURL));
}

void ChromeOsFeedbackDelegate::OpenSystemInfoDialog() {
  // TODO(http://b/239701119): Make the sys_info.html page a separate WebUI.
  // For now, use the old Feedback tool's sys_info.html.
  GURL systemInfoUrl =
      GURL(base::StrCat({chrome::kChromeUIFeedbackURL, "html/sys_info.html"}));
  OpenWebDialog(systemInfoUrl);
}

bool ChromeOsFeedbackDelegate::IsChildAccount() {
  return profile_->IsChild();
}

void ChromeOsFeedbackDelegate::OpenWebDialog(GURL url) {
  Browser* feedback_browser = ash::FindSystemWebAppBrowser(
      profile_, ash::SystemWebAppType::OS_FEEDBACK);

  gfx::NativeWindow window = feedback_browser->window()->GetNativeWindow();

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);

  ChildWebDialog* child_dialog = new ChildWebDialog(
      profile_, widget, url,
      /*title=*/std::u16string(),
      /*modal_type=*/ui::MODAL_TYPE_NONE, /*dialog_width=*/640,
      /*dialog_height=*/400, /*can_resize=*/true,
      /*can_minimize=*/true);

  child_dialog->Show();
}

void ChromeOsFeedbackDelegate::PreloadSystemLogs() {
  base::TimeTicks fetch_start_time = base::TimeTicks::Now();
  feedback_service_->GetFeedbackPrivateDelegate()->FetchSystemInformation(
      profile_,
      base::BindOnce(&ChromeOsFeedbackDelegate::PreloadSystemLogsDone,
                     weak_ptr_factory_.GetWeakPtr(), fetch_start_time));
}

void ChromeOsFeedbackDelegate::PreloadSystemLogsDone(
    base::TimeTicks fetch_start_time,
    std::unique_ptr<system_logs::SystemLogsResponse> response) {
  base::UmaHistogramMediumTimes("Feedback.Duration.FetchSystemInformation",
                                base::TimeTicks::Now() - fetch_start_time);
  system_logs_response_ = std::move(response);
}

}  // namespace ash

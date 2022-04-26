// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>
#include <utility>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/strings/utf_string_conversions.h"
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
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

namespace {

feedback::FeedbackUploader* GetFeedbackUploaderForContext(
    content::BrowserContext* context) {
  return feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context);
}

}  // namespace

using ::ash::os_feedback_ui::mojom::SendReportStatus;

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(Profile* profile)
    : profile_(profile) {
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

void ChromeOsFeedbackDelegate::SendReport(
    os_feedback_ui::mojom::ReportPtr report,
    SendReportCallback callback) {
  base::WeakPtr<feedback::FeedbackUploader> uploader;
  if (feedback_uploader_for_testing_) {
    uploader = base::AsWeakPtr(feedback_uploader_for_testing_.get());
  } else {
    uploader = base::AsWeakPtr(GetFeedbackUploaderForContext(profile_));
  }
  scoped_refptr<feedback::FeedbackData> feedback_data =
      base::MakeRefCounted<feedback::FeedbackData>(
          std::move(uploader), ContentTracingManager::Get());

  feedback_data->set_locale(GetApplicationLocale());
  // TODO(xiangdongkong): Add UserAgent.

  feedback_data->set_description(base::UTF16ToUTF8(report->description));

  const auto& feedback_context = report->feedback_context;
  if (feedback_context->email.has_value()) {
    feedback_data->set_user_email(feedback_context->email.value());
  }
  if (feedback_context->page_url.has_value()) {
    feedback_data->set_page_url(feedback_context->page_url.value().spec());
  }
  // TODO(xiangdongkong): Fetch system logs if needed.

  // Signal the feedback object that the data from the feedback page has been
  // filled - the object will manage sending of the actual report.
  feedback_data->OnFeedbackPageDataComplete();

  const SendReportStatus status = net::NetworkChangeNotifier::IsOffline()
                                      ? SendReportStatus::kDelayed
                                      : SendReportStatus::kSuccess;
  std::move(callback).Run(status);
}

void ChromeOsFeedbackDelegate::SetFeedbackUploaderForTesting(
    feedback::FeedbackUploader* uploader) {
  feedback_uploader_for_testing_ = uploader;
}

}  // namespace ash

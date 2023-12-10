// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/input_method/editor_identity_utils.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash::input_method {
namespace {

constexpr int kOrcaFeedbackProductId = 5314436;

base::WeakPtr<feedback::FeedbackUploader> GetFeedbackUploaderFromContext(
    content::BrowserContext* context) {
  return base::AsWeakPtr(static_cast<feedback::FeedbackUploader*>(
      feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context)));
}

std::string GetChromeVersion() {
  return chrome::GetVersionString(chrome::WithExtendedStable(true));
}

std::string GetOsVersion() {
  std::string version;
  base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_VERSION", &version);
  return version;
}

scoped_refptr<feedback::FeedbackData> RedactFeedbackData(
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  redaction::RedactionTool redactor(nullptr);
  redactor.EnableCreditCardRedaction(true);
  feedback_data->RedactDescription(redactor);
  return feedback_data;
}

void SendFeedback(scoped_refptr<feedback::FeedbackData> feedback_data) {
  feedback_data->OnFeedbackPageDataComplete();
}

void RedactThenSendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RedactFeedbackData, feedback_data),
      base::BindOnce(&SendFeedback));
}

}  // namespace

bool SendEditorFeedback(Profile* profile, std::string_view description) {
  if (!base::FeatureList::IsEnabled(features::kOrcaFeedback)) {
    return false;
  }

  auto feedback_data = base::MakeRefCounted<feedback::FeedbackData>(
      GetFeedbackUploaderFromContext(profile), nullptr);
  feedback_data->set_product_id(kOrcaFeedbackProductId);
  feedback_data->set_include_chrome_platform(false);
  feedback_data->set_description(std::string(description));
  if (absl::optional<std::string> user_email =
          GetSignedInUserEmailFromProfile(profile);
      user_email.has_value() &&
      gaia::IsGoogleInternalAccountEmail(*user_email)) {
    feedback_data->set_user_email(*user_email);
  }
  feedback_data->AddLog("CHROME VERSION", GetChromeVersion());
  feedback_data->AddLog("CHROMEOS_RELEASE_VERSION", GetOsVersion());
  RedactThenSendFeedback(feedback_data);
  return true;
}
}  // namespace ash::input_method

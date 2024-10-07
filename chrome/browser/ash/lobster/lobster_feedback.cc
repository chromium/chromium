// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/feedback/feedback_constants.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

namespace {

base::WeakPtr<feedback::FeedbackUploader> GetFeedbackUploaderFromContext(
    content::BrowserContext* context) {
  feedback::FeedbackUploader* uploader =
      static_cast<feedback::FeedbackUploader*>(
          feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(
              context));
  // Can be a nullptr value in unit tests.
  if (!uploader) {
    return nullptr;
  }

  return uploader->AsWeakPtr();
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

std::string BuildFeedbackDescription(std::string_view query,
                                     std::string_view model_version,
                                     std::string_view user_description) {
  return base::StringPrintf(
      "model_input: %s\nmodel_version: %s\nuser_description: %s", query,
      model_version, user_description);
}

}  // namespace

bool SendLobsterFeedback(Profile* profile,
                         std::string_view query,
                         std::string_view model_version,
                         std::string_view user_description,
                         std::string_view image_bytes) {
  auto feedback_data = base::MakeRefCounted<feedback::FeedbackData>(
      GetFeedbackUploaderFromContext(profile), nullptr);

  feedback_data->set_product_id(feedback::kLobsterFeedbackProductId);
  feedback_data->set_include_chrome_platform(false);
  feedback_data->set_description(
      BuildFeedbackDescription(query, model_version, user_description));
  feedback_data->set_image(image_bytes.data());
  feedback_data->AddLog("CHROME VERSION", GetChromeVersion());
  feedback_data->AddLog("CHROMEOS_RELEASE_VERSION", GetOsVersion());

  RedactThenSendFeedback(feedback_data);

  return true;
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_

#include "ash/webui/recorder_app_ui/recorder_app_ui_delegate.h"
#include "components/soda/constants.h"
#include "content/public/browser/web_ui.h"

/**
 * Implementation of the RecorderAppUIDelegate interface. Provides the recorder
 * app code in ash/ with functions that only exist in chrome/.
 */
class ChromeRecorderAppUIDelegate : public ash::RecorderAppUIDelegate {
 public:
  explicit ChromeRecorderAppUIDelegate(content::WebUI* web_ui);

  ChromeRecorderAppUIDelegate(const ChromeRecorderAppUIDelegate&) = delete;
  ChromeRecorderAppUIDelegate& operator=(const ChromeRecorderAppUIDelegate&) =
      delete;

  // ash::RecorderAppUIDelegate
  void InstallSoda(speech::LanguageCode language_code) override;

  void OpenAiFeedbackDialog(const std::string& description_template) override;

  bool CanUseGenerativeAiForCurrentProfile() override;

  bool CanUseSpeakerLabelForCurrentProfile() override;

  void RecordSpeakerLabelConsent(
      const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& consent)
      override;

  media_device_salt::MediaDeviceSaltService* GetMediaDeviceSaltService(
      content::BrowserContext* context) override;

 private:
  raw_ptr<content::WebUI> web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_CHROME_RECORDER_APP_UI_DELEGATE_H_

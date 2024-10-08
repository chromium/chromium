// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_
#define ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_

#include "components/soda/constants.h"
#include "components/sync/protocol/user_consent_types.pb.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_device_salt {
class MediaDeviceSaltService;
}  // namespace media_device_salt

namespace ash {
// A delegate which exposes browser functionality from //chrome to the recorder
// app ui page handler.
class RecorderAppUIDelegate {
 public:
  virtual void InstallSoda(speech::LanguageCode language_code) = 0;

  virtual void OpenAiFeedbackDialog(
      const std::string& description_template) = 0;

  virtual bool CanUseGenerativeAiForCurrentProfile() = 0;

  virtual bool CanUseSpeakerLabelForCurrentProfile() = 0;

  virtual void RecordSpeakerLabelConsent(
      const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent&
          consent) = 0;

  virtual ~RecorderAppUIDelegate() = default;

  // Returns a service that provides persistent salts for generating media
  // device IDs. Can be null if the embedder does not support persistent salts.
  virtual media_device_salt::MediaDeviceSaltService* GetMediaDeviceSaltService(
      content::BrowserContext* context) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_RECORDER_APP_UI_DELEGATE_H_

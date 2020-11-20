// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/tts_handler.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
TtsHandler::TtsHandler(Profile* profile) : profile_(profile) {}
TtsHandler::~TtsHandler() = default;

void TtsHandler::Announce(const std::string& text,
                          const base::TimeDelta delay) {
  const bool chrome_vox_enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);
  if (!chrome_vox_enabled)
    return;

  delay_timer_ = std::make_unique<base::OneShotTimer>();
  delay_timer_->Start(
      FROM_HERE, delay,
      base::BindOnce(&TtsHandler::Speak, base::Unretained(this), text));
}

void TtsHandler::OnTtsEvent(content::TtsUtterance* utterance,
                            content::TtsEventType event_type,
                            int char_index,
                            int length,
                            const std::string& error_message) {}

void TtsHandler::Speak(const std::string& text) {
  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(profile_);
  utterance->SetText(text);
  utterance->SetEventDelegate(this);
  utterance->SetShouldClearQueue(false);

  auto* tts_controller = content::TtsController::GetInstance();
  tts_controller->SpeakOrEnqueue(std::move(utterance));
}
}  // namespace chromeos

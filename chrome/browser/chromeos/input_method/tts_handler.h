// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TTS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TTS_HANDLER_H_

#include <string>

#include "base/timer/timer.h"
#include "content/public/browser/tts_controller.h"

class Profile;

namespace chromeos {

class TtsHandler : public content::UtteranceEventDelegate {
 public:
  explicit TtsHandler(Profile* profile);
  ~TtsHandler() override;

  // Announce |text| after some |delay|. The delay is to avoid conflict with
  // other ChromeVox announcements. This should be no-op if ChromeVox is not
  // enabled.
  void Announce(const std::string& text,
                const base::TimeDelta delay = base::TimeDelta());

  // UtteranceEventDelegate implementation.
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

 private:
  virtual void Speak(const std::string& text);

  Profile* const profile_;
  std::unique_ptr<base::OneShotTimer> delay_timer_;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TTS_HANDLER_H_

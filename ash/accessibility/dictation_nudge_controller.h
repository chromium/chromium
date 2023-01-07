// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DICTATION_NUDGE_CONTROLLER_H_
#define ASH_ACCESSIBILITY_DICTATION_NUDGE_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge_controller.h"

namespace ash {

// Class that manages showing a nudge explaining that Dictation language has
// been upgraded in the background to work offline.
class ASH_EXPORT DictationNudgeController : public SystemNudgeController {
 public:
  // Creates the Dictation nudge for the user's preferred recognition locale,
  // |dictation_locale|. Uses the |application_locale| to get the human-readable
  // name for the |dictation_locale|.
  DictationNudgeController(const std::string& dictation_locale,
                           const std::string& application_locale);
  DictationNudgeController(const DictationNudgeController&) = delete;
  DictationNudgeController& operator=(const DictationNudgeController&) = delete;
  ~DictationNudgeController() override;

  std::string dictation_locale() const { return dictation_locale_; }
  std::string application_locale() const { return application_locale_; }

 protected:
  // SystemNudgeController:
  std::unique_ptr<SystemNudge> CreateSystemNudge() override;

 private:
  // The locale in which the user's speech recognition is processed for
  // Dictation.
  const std::string dictation_locale_;

  // The application locale for Chrome OS.
  const std::string application_locale_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DICTATION_NUDGE_CONTROLLER_H_

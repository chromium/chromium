// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_consent_store.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/ash/text_input_method.h"

namespace ash::input_method {

// EditorSwitch is the centralized switch that decides whether the feature is
// available for use, and if available, further decides whether the feature
// should be popped up given a particular input context.
class EditorSwitch {
 public:
  class Observer {
   public:
    virtual void OnEditorModeChanged(const EditorMode& mode) = 0;
  };

  EditorSwitch(Observer* observer, Profile* profile, EditorContext* context);
  EditorSwitch(const EditorSwitch&) = delete;
  EditorSwitch& operator=(const EditorSwitch&) = delete;
  ~EditorSwitch();

  // Determines if the feature trace is ever allowed to be visible.
  bool IsAllowedForUse() const;

  bool IsFeedbackEnabled() const;

  bool CanShowNoticeBanner() const;

  EditorMode GetEditorMode() const;

  EditorOpportunityMode GetEditorOpportunityMode() const;

  std::vector<EditorBlockedReason> GetBlockedReasons() const;

  void OnContextUpdated();

 private:
  // Determines if the feature can be triggered from an input context. If it is
  // not allowed for use, then returns false.
  bool CanBeTriggered() const;

  raw_ptr<Observer> observer_;
  raw_ptr<Profile> profile_;
  raw_ptr<EditorContext> context_;

  const std::vector<std::string> ime_allowlist_;

  // Used to determine when Observer::OnEditorModeChanged should be called.
  EditorMode last_known_editor_mode_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

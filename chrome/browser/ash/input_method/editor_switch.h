// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

#include "ui/base/ime/ash/text_input_method.h"

namespace ash::input_method {

// EditorSwitch is the centralized switch that decides whether the feature is
// available for use, and if available, further decides whether the feature
// should be popped up given a particular input context.
class EditorSwitch {
 public:
  explicit EditorSwitch(bool is_managed);
  EditorSwitch(const EditorSwitch&) = delete;
  EditorSwitch& operator=(const EditorSwitch&) = delete;
  ~EditorSwitch();

  // Determines if the feature trace is ever allowed to be visible.
  bool IsAllowedForUse();

  // Determines if the feature can be triggered from an input context. If it is
  // not allowed for use, then returns false.
  bool CanBeTriggered();

  // Handles the change in input context.
  void OnInputContextUpdated(
      const TextInputMethod::InputContext& input_context);

  void OnActivateIme(std::string_view engine_id);

 private:
  void UpdateTriggerableCache();

  bool is_allowed_for_use_ = false;
  bool can_be_triggered_ = false;

  std::string active_engine_id_;
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

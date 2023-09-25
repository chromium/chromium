// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

#include "ash/constants/app_types.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/ash/text_input_method.h"

namespace ash::input_method {

// EditorSwitch is the centralized switch that decides whether the feature is
// available for use, and if available, further decides whether the feature
// should be popped up given a particular input context.
class EditorSwitch {
 public:
  // country_code in the lowercase ISO 3166-1 alpha-2 format to determine
  // the country where the device is situated.
  EditorSwitch(Profile* profile, std::string_view country_code);
  EditorSwitch(const EditorSwitch&) = delete;
  EditorSwitch& operator=(const EditorSwitch&) = delete;
  ~EditorSwitch();

  // Determines if the feature trace is ever allowed to be visible.
  bool IsAllowedForUse() const;

  // Handles the change in input context.
  void OnInputContextUpdated(
      const TextInputMethod::InputContext& input_context,
      const TextFieldContextualInfo& text_field_contextual_info);

  void OnActivateIme(std::string_view engine_id);

  void OnTabletModeUpdated(bool tablet_mode_enabled);

  void OnTextSelectionLengthChanged(size_t new_length);

  void SetProfile(Profile* profile);

  EditorMode GetEditorMode() const;

 private:
  raw_ptr<Profile> profile_;

  // Determines if the feature can be triggered from an input context. If it is
  // not allowed for use, then returns false.
  bool CanBeTriggered() const;

  std::string country_code_;
  std::string active_engine_id_;
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  ash::AppType app_type_ = ash::AppType::NON_APP;
  bool tablet_mode_enabled_ = false;
  size_t text_length_ = 0;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_SWITCH_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_TEST_IME_CONTROLLER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_TEST_IME_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ime_controller.h"
#include "ash/public/cpp/ime_info.h"
#include "base/memory/raw_ptr.h"

// Class that resets the ImeController instance to nullptr and then restores it
// when it is destroyed.
class ImeControllerResetterForTest {
 public:
  ImeControllerResetterForTest();
  ~ImeControllerResetterForTest();

 private:
  const raw_ptr<ash::ImeController> instance_;
};

class TestImeController : private ImeControllerResetterForTest,
                          public ash::ImeController {
 public:
  TestImeController();

  TestImeController(const TestImeController&) = delete;
  TestImeController& operator=(const TestImeController&) = delete;

  ~TestImeController() override;

  // ash::ImeController:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SetClient(ash::ImeControllerClient* client) override;
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<ash::ImeInfo> available_imes,
                  std::vector<ash::ImeMenuItem> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;
  void UpdateCapsLockState(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;
  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override;
  void ShowModeIndicator(const gfx::Rect& anchor_bounds,
                         const std::u16string& text) override;
  bool IsCapsLockEnabled() const override;

  // The most recent values received via mojo.
  std::string current_ime_id_;
  std::vector<ash::ImeInfo> available_imes_;
  std::vector<ash::ImeMenuItem> menu_items_;
  bool managed_by_policy_ = false;
  bool show_ime_menu_on_shelf_ = false;
  bool show_mode_indicator_ = false;
  bool is_caps_lock_enabled_ = false;
  std::string keyboard_layout_name_;
  bool is_extra_input_options_enabled_ = false;
  bool is_emoji_enabled_ = false;
  bool is_handwriting_enabled_ = false;
  bool is_voice_enabled_ = false;
};

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_TEST_IME_CONTROLLER_H_

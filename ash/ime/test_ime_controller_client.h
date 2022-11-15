// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_TEST_IME_CONTROLLER_CLIENT_H_
#define ASH_IME_TEST_IME_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/ime_controller_client.h"

namespace ash {

class TestImeControllerClient : public ImeControllerClient {
 public:
  TestImeControllerClient();

  TestImeControllerClient(const TestImeControllerClient&) = delete;
  TestImeControllerClient& operator=(const TestImeControllerClient&) = delete;

  ~TestImeControllerClient();

  // ImeControllerClient:
  void SwitchToNextIme() override;
  void SwitchToLastUsedIme() override;
  void SwitchImeById(const std::string& id, bool show_message) override;
  void ActivateImeMenuItem(const std::string& key) override;
  void SetCapsLockEnabled(bool enabled) override;
  void OverrideKeyboardKeyset(input_method::ImeKeyset keyset,
                              OverrideKeyboardKeysetCallback callback) override;
  void ShowModeIndicator() override;

  int next_ime_count_ = 0;
  int last_used_ime_count_ = 0;
  int switch_ime_count_ = 0;
  int set_caps_lock_count_ = 0;
  std::string last_switch_ime_id_;
  bool last_show_message_ = false;
  input_method::ImeKeyset last_keyset_ = input_method::ImeKeyset::kNone;
  int show_mode_indicator_count_ = 0;
};

}  // namespace ash

#endif  // ASH_IME_TEST_IME_CONTROLLER_CLIENT_H_

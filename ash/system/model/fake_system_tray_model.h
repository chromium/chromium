// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_FAKE_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_FAKE_SYSTEM_TRAY_MODEL_H_

#include "ash/ash_export.h"
#include "ash/system/model/system_tray_model.h"

namespace ash {

// A fake implementation of `SystemTrayModel`. Used for mocking in the status
// area internals testing page.
class ASH_EXPORT FakeSystemTrayModel : public SystemTrayModel {
 public:
  FakeSystemTrayModel();

  FakeSystemTrayModel(const FakeSystemTrayModel&) = delete;
  FakeSystemTrayModel& operator=(const FakeSystemTrayModel&) = delete;

  ~FakeSystemTrayModel() override;

  // SystemTrayModel:
  bool IsFakeModel() const override;
  bool IsInUserChildSession() const override;

  void set_is_in_user_child_session(bool is_in_user_child_session) {
    is_in_user_child_session_ = is_in_user_child_session;
  }

 private:
  bool is_in_user_child_session_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_FAKE_SYSTEM_TRAY_MODEL_H_

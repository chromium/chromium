// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_SCOPED_FAKE_SYSTEM_TRAY_MODEL_H_
#define ASH_SYSTEM_MODEL_SCOPED_FAKE_SYSTEM_TRAY_MODEL_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class FakeSystemTrayModel;
class SystemTrayModel;

// Creates a fake `SystemTrayModel` and use that fake instance to be the
// singleton object for `Shell::Get()->system_tray_model()`. The real instance
// will be restored when this scoped object is destructed.
class ASH_EXPORT ScopedFakeSystemTrayModel {
 public:
  ScopedFakeSystemTrayModel();

  ScopedFakeSystemTrayModel(const ScopedFakeSystemTrayModel&) = delete;
  ScopedFakeSystemTrayModel& operator=(const ScopedFakeSystemTrayModel&) =
      delete;

  ~ScopedFakeSystemTrayModel();

  FakeSystemTrayModel* fake_model() { return fake_model_; }

 private:
  static ScopedFakeSystemTrayModel* instance_;

  std::unique_ptr<SystemTrayModel> real_system_tray_model_instance_;

  raw_ptr<FakeSystemTrayModel> fake_model_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_SCOPED_FAKE_SYSTEM_TRAY_MODEL_H_

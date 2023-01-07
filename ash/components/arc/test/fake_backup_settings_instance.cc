// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_backup_settings_instance.h"

namespace arc {

FakeBackupSettingsInstance::FakeBackupSettingsInstance() = default;

FakeBackupSettingsInstance::~FakeBackupSettingsInstance() = default;

void FakeBackupSettingsInstance::ClearCallHistory() {
  set_backup_enabled_count_ = 0;
}

void FakeBackupSettingsInstance::SetBackupEnabled(bool enabled, bool managed) {
  ++set_backup_enabled_count_;
  enabled_ = enabled;
  managed_ = managed;
}

}  // namespace arc

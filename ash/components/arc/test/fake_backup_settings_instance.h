// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_BACKUP_SETTINGS_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_BACKUP_SETTINGS_INSTANCE_H_

#include "ash/components/arc/mojom/backup_settings.mojom.h"

namespace arc {

class FakeBackupSettingsInstance : public mojom::BackupSettingsInstance {
 public:
  FakeBackupSettingsInstance();

  FakeBackupSettingsInstance(const FakeBackupSettingsInstance&) = delete;
  FakeBackupSettingsInstance& operator=(const FakeBackupSettingsInstance&) =
      delete;

  ~FakeBackupSettingsInstance() override;

  // mojom::BackupSettingsInstance overrides:
  void SetBackupEnabled(bool enabled, bool managed) override;

  void ClearCallHistory();

  int set_backup_enabled_count() const { return set_backup_enabled_count_; }
  bool enabled() const { return enabled_; }
  bool managed() const { return managed_; }

 private:
  int set_backup_enabled_count_ = 0;
  bool enabled_ = false;
  bool managed_ = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_BACKUP_SETTINGS_INSTANCE_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_MIGRATION_PROGRESS_TRACKER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_MIGRATION_PROGRESS_TRACKER_H_

#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"

namespace ash {
class FakeMigrationProgressTracker : public MigrationProgressTracker {
 public:
  FakeMigrationProgressTracker() = default;
  ~FakeMigrationProgressTracker() override = default;
  FakeMigrationProgressTracker(const FakeMigrationProgressTracker&) = delete;
  FakeMigrationProgressTracker& operator=(const FakeMigrationProgressTracker&) =
      delete;

  void UpdateProgress(int64_t size) override {}
  void SetTotalSizeToCopy(int64_t size) override {}
};
}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_MIGRATION_PROGRESS_TRACKER_H_

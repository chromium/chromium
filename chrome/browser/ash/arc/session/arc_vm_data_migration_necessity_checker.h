// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_VM_DATA_MIGRATION_NECESSITY_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_VM_DATA_MIGRATION_NECESSITY_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace arc {

// Class to check whether /data migration needs to be performed for enabling
// virtio-blk /data on ARCVM.
class ArcVmDataMigrationNecessityChecker {
 public:
  explicit ArcVmDataMigrationNecessityChecker(Profile* profile);
  ArcVmDataMigrationNecessityChecker(
      const ArcVmDataMigrationNecessityChecker&) = delete;
  ArcVmDataMigrationNecessityChecker& operator=(
      const ArcVmDataMigrationNecessityChecker&) = delete;
  ~ArcVmDataMigrationNecessityChecker();

  using CheckCallback = base::OnceCallback<void(absl::optional<bool> result)>;

  // Checks whether /data migration needs to be performed. When the migration is
  // necessary/unnecessary, |callback| is called with true/false, respectively.
  // On error, |callback| is called with absl::nullopt.
  // Should be called when ARCVM /data migration is enabled.
  void Check(CheckCallback callback);

 private:
  void OnArcVmDataMigratorStarted(CheckCallback callback, bool result);

  void OnHasDataToMigrateResponse(CheckCallback callback,
                                  absl::optional<bool> response);

  const raw_ptr<Profile, ExperimentalAsh> profile_;

  base::WeakPtrFactory<ArcVmDataMigrationNecessityChecker> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_VM_DATA_MIGRATION_NECESSITY_CHECKER_H_

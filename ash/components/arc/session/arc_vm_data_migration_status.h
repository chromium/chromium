// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_STATUS_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_STATUS_H_

#include <ostream>

namespace arc {

constexpr char kArcVmDataMigrationStatusOnArcStartedHistogramName[] =
    "Arc.VmDataMigration.MigrationStatusOnArcStarted";

constexpr char kArcVmDataMigrationFinishReasonHistogramName[] =
    "Arc.VmDataMigration.MigrationFinishReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ArcVmDataMigrationStatus" in tools/metrics/histograms/enums.xml.
enum class ArcVmDataMigrationStatus {
  // The user has not been notified of the /data migration.
  kUnnotified = 0,
  // The user has been notified of the availability of the /data migration via a
  // notification.
  kNotified = 1,
  // The user has confirmed the migration. The actual migration will be
  // triggered after a Chrome restart.
  kConfirmed = 2,
  // The migration has been started and will be resumed upon the next login.
  kStarted = 3,
  // The migration has been completed and not needed anymore.
  kFinished = 4,
  kMaxValue = kFinished,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ArcVmDataMigrationFinishReason" in tools/metrics/histograms/enums.xml.
enum class ArcVmDataMigrationFinishReason {
  // There is no /data to migrate, so virtio-blk can be just enabled.
  kNoDataToMigrate = 0,
  // Migration finishes successfully.
  kMigrationSuccess = 1,
  // Migration fails, resulting in enabling virtio-blk on wiped /data.
  kMigrationFailure = 2,
  kMaxValue = kMigrationFailure,
};

enum class ArcVmDataMigrationStrategy {
  // The user should not be prompted to go through /data migration.
  kDoNotPrompt = 0,
  // The user should be prompted to go through /data migration via
  // a notification.
  kPrompt = 1,
  kMaxValue = kPrompt,
};

std::ostream& operator<<(std::ostream& os, ArcVmDataMigrationStatus status);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_VM_DATA_MIGRATION_STATUS_H_

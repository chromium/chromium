// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_vm_data_migration_status.h"

namespace arc {

std::ostream& operator<<(std::ostream& os, ArcVmDataMigrationStatus status) {
  switch (status) {
    case ArcVmDataMigrationStatus::kUnnotified:
      return os << "Unnotified";
    case ArcVmDataMigrationStatus::kNotified:
      return os << "Notified";
    case ArcVmDataMigrationStatus::kConfirmed:
      return os << "Confirmed";
    case ArcVmDataMigrationStatus::kStarted:
      return os << "Started";
    case ArcVmDataMigrationStatus::kFinished:
      return os << "Finished";
  }
}

}  // namespace arc

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_CONSTANTS_H_

#include "base/byte_count.h"

namespace arc {

// The minimum battery level to start the migration.
inline constexpr double kMigrationMinimumBatteryPercent = 10;

// The minimum size of available space to start the migration.
inline constexpr base::ByteCount kMigrationMinimumAvailableStorage =
    base::MiB(50);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_CONSTANTS_H_

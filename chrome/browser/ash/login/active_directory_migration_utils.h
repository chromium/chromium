// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ACTIVE_DIRECTORY_MIGRATION_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_ACTIVE_DIRECTORY_MIGRATION_UTILS_H_

#include "base/functional/callback.h"

namespace ash {

// Utils that provides functionalities related to skipping some OOBE screens
// during the migration of Chromad devices into cloud management.
// TODO(crbug/1298038): Remove this file and it's usage when Chromad to clould
// migration is complete.
namespace ad_migration_utils {

void CheckChromadMigrationOobeFlow(base::OnceCallback<void(bool)> callback);

}  // namespace ad_migration_utils
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ACTIVE_DIRECTORY_MIGRATION_UTILS_H_

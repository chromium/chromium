// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_

#include "base/bind.h"
#include "base/files/file_path.h"

class Profile;

namespace chromeos {

bool IsAccountManagerAvailable(const Profile* const profile);

// Initializes account manager if it has not been initialized yet. Safe to call
// multiple times. |cryptohome_root_dir| is root of user's home partition (same
// as the Profile directory). |initialization_callback| is used by the caller to
// inform itself about a successful initialization.
void InitializeAccountManager(const base::FilePath& cryptohome_root_dir,
                              base::OnceClosure initialization_callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_

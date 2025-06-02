// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

bool IsAccountManagerAvailable(Profile* profile);

// Initializes account manager if it has not been initialized yet. Safe to call
// multiple times. |cryptohome_root_dir| is root of user's home partition (same
// as the Profile directory). |initialization_callback| is used by the caller to
// inform itself about a successful initialization.
void InitializeAccountManager(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    const base::FilePath& cryptohome_root_dir,
    base::OnceClosure initialization_callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_UTIL_H_

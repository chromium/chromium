// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password store.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_

#include "base/memory/scoped_refptr.h"

namespace password_manager {
class PasswordStoreInterface;
}

class Profile;

// Returns the password store associated with the currently active profile.
password_manager::PasswordStoreInterface* GetPasswordStore(
    Profile* profile,
    bool use_account_store);

// Query the password stores and reports multiple metrics. The actual reporting
// is delayed by 30 seconds, to ensure it doesn't happen during the "hot phase"
// of Chrome startup.
void DelayReportingPasswordStoreMetrics(Profile* profile);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_UTILS_H_

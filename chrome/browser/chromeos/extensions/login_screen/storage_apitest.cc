// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "components/version_info/version_info.h"

namespace {

const char kCannotAccessLocalStorage[] = "StorageCannotAccessLocalStorage";
const char kCannotAccessSyncStorage[] = "StorageCannotAccessSyncStorage";
const char kCanAccessManagedStorage[] = "StorageCanAccessManagedStorage";

}  // namespace

namespace chromeos {

class StorageApitest : public LoginScreenApitestBase {
 public:
  StorageApitest() : LoginScreenApitestBase(version_info::Channel::DEV) {}
  ~StorageApitest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(StorageApitest);
};

IN_PROC_BROWSER_TEST_F(StorageApitest, CannotAccessLocalStorage) {
  SetUpExtensionAndRunTest(kCannotAccessLocalStorage);
}

IN_PROC_BROWSER_TEST_F(StorageApitest, CannotAccessSyncStorage) {
  SetUpExtensionAndRunTest(kCannotAccessSyncStorage);
}

IN_PROC_BROWSER_TEST_F(StorageApitest, CanAccessManagedStorage) {
  SetUpExtensionAndRunTest(kCanAccessManagedStorage);
}

}  // namespace chromeos

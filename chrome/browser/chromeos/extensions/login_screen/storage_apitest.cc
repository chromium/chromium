// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

namespace {

const char kCannotAccessLocalStorage[] = "StorageCannotAccessLocalStorage";
const char kCannotAccessSyncStorage[] = "StorageCannotAccessSyncStorage";
const char kCanAccessManagedStorage[] = "StorageCanAccessManagedStorage";

}  // namespace

namespace chromeos {

class StorageApitest : public LoginScreenApitestBase {
 public:
  StorageApitest() : LoginScreenApitestBase(version_info::Channel::DEV) {}

  StorageApitest(const StorageApitest&) = delete;
  StorageApitest& operator=(const StorageApitest&) = delete;

  ~StorageApitest() override = default;
};

IN_PROC_BROWSER_TEST_F(StorageApitest, CannotAccessLocalStorage) {
  SetUpLoginScreenExtensionAndRunTest(kCannotAccessLocalStorage);
}

IN_PROC_BROWSER_TEST_F(StorageApitest, CannotAccessSyncStorage) {
  SetUpLoginScreenExtensionAndRunTest(kCannotAccessSyncStorage);
}

IN_PROC_BROWSER_TEST_F(StorageApitest, CanAccessManagedStorage) {
  SetUpLoginScreenExtensionAndRunTest(kCanAccessManagedStorage);
}

}  // namespace chromeos

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace crosapi {

class CrosapiManager;

// Sets up classes needed by tests that depend on CrosapiManager.
class TestCrosapiEnvironment {
 public:
  TestCrosapiEnvironment();
  ~TestCrosapiEnvironment();

  void SetUp();
  void TearDown();

  TestingProfileManager* profile_manager() { return &testing_profile_manager_; }

 private:
  // If we want to use `this` in a test fixture that provides its own
  // ProfileManager, then we'd want to allow its injection into `this` to avoid
  // having multiple ProfileManager instances.
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  bool initialized_login_state_ = false;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_

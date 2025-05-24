// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace crosapi {

class CrosapiManager;

// Sets up classes needed by tests that depend on CrosapiManager.
class TestCrosapiEnvironment {
 public:
  // `testing_profile_manager_` must be non-null and must be outlive `this`.
  explicit TestCrosapiEnvironment(
      TestingProfileManager* testing_profile_manager_);
  ~TestCrosapiEnvironment();

  void SetUp();
  void TearDown();

 private:
  // CrosapiAsh depends on ProfileManager.
  const raw_ref<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  bool initialized_login_state_ = false;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_ENVIRONMENT_H_

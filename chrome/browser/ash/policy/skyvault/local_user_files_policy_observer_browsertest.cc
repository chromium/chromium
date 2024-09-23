// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

namespace {

class TestObserver : LocalUserFilesPolicyObserver {
 public:
  void OnLocalUserFilesPolicyChanged() override {
    local_user_files_allowed_ = g_browser_process->local_state()->GetBoolean(
        prefs::kLocalUserFilesAllowed);
  }

  bool local_user_files_allowed() { return local_user_files_allowed_; }

 private:
  bool local_user_files_allowed_;
};

}  // namespace

class LocalUserFilesPolicyObserverTest : public policy::PolicyTest {
 public:
  LocalUserFilesPolicyObserverTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSkyVault);
  }
  ~LocalUserFilesPolicyObserverTest() override = default;

 protected:
  void SetPolicyValue(bool local_user_files_allowed) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kLocalUserFilesAllowed,
                                  base::Value(local_user_files_allowed));
    provider_.UpdateChromePolicy(policies);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalUserFilesPolicyObserverTest, CheckPolicyValue) {
  TestObserver observer;

  SetPolicyValue(/*local_user_files_allowed=*/true);
  ASSERT_TRUE(observer.local_user_files_allowed());

  SetPolicyValue(/*local_user_files_allowed=*/false);
  ASSERT_FALSE(observer.local_user_files_allowed());
}

}  // namespace policy::local_user_files

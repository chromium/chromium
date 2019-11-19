// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_check.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/preload_check.h"
#include "extensions/browser/preload_check_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlacklistCheckTest : public testing::Test {
 public:
  BlacklistCheckTest()
      : test_prefs_(base::ThreadTaskRunnerHandle::Get()),
        blacklist_(test_prefs_.prefs()) {}

 protected:
  void SetUp() override {
    test_blacklist_.Attach(&blacklist_);
    extension_ = test_prefs_.AddExtension("foo");
  }

  void SetBlacklistState(BlacklistState state) {
    test_blacklist_.SetBlacklistState(extension_->id(), state, /*notify=*/true);
  }

  Blacklist* blacklist() { return &blacklist_; }
  scoped_refptr<Extension> extension_;
  PreloadCheckRunner runner_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestExtensionPrefs test_prefs_;
  Blacklist blacklist_;
  TestBlacklist test_blacklist_;
};

}  // namespace

// Tests that the blacklist check identifies a blacklisted extension.
TEST_F(BlacklistCheckTest, BlacklistedMalware) {
  SetBlacklistState(BLACKLISTED_MALWARE);

  BlacklistCheck check(blacklist(), extension_);
  runner_.RunUntilComplete(&check);

  EXPECT_THAT(runner_.errors(),
              testing::UnorderedElementsAre(PreloadCheck::BLACKLISTED_ID));
  EXPECT_TRUE(check.GetErrorMessage().empty());
}

// Tests that the blacklist check ignores a non-blacklisted extension.
TEST_F(BlacklistCheckTest, Pass) {
  SetBlacklistState(NOT_BLACKLISTED);

  BlacklistCheck check(blacklist(), extension_);
  runner_.RunUntilComplete(&check);

  EXPECT_EQ(0u, runner_.errors().size());
  EXPECT_TRUE(check.GetErrorMessage().empty());
}

// Tests that destroying the check after starting it does not cause errors.
TEST_F(BlacklistCheckTest, ResetCheck) {
  SetBlacklistState(BLACKLISTED_MALWARE);

  {
    BlacklistCheck check(blacklist(), extension_);
    runner_.Run(&check);
  }

  runner_.WaitForIdle();
  EXPECT_FALSE(runner_.called());
}

}  // namespace extensions

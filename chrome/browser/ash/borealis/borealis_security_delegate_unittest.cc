// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_security_delegate.h"

#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisSecurityDelegateTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_env_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(BorealisSecurityDelegateTest, MainAppCanSelfActivate) {
  CreateFakeMainApp(&profile_);
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.wmclass.Steam",
      &BorealisServiceFactory::GetForProfile(&profile_)->WindowManager());
  EXPECT_TRUE(
      BorealisSecurityDelegate::MakeForTesting(&profile_)->CanSelfActivate(
          window->window()));
}

TEST_F(BorealisSecurityDelegateTest, NormalAppCanNotSelfActivate) {
  CreateFakeApp(&profile_, "not_steam", "borealis/123");
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.wmclass.not_steam",
      &BorealisServiceFactory::GetForProfile(&profile_)->WindowManager());

  ASSERT_FALSE(BorealisWindowManager::IsAnonymousAppId(
      BorealisServiceFactory::GetForProfile(&profile_)
          ->WindowManager()
          .GetShelfAppId(window->window())));

  EXPECT_FALSE(
      BorealisSecurityDelegate::MakeForTesting(&profile_)->CanSelfActivate(
          window->window()));
}

TEST_F(BorealisSecurityDelegateTest, AnonymousAppCanNotSelfActivate) {
  std::unique_ptr<ScopedTestWindow> window = MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.wmclass.anonymous",
      &BorealisServiceFactory::GetForProfile(&profile_)->WindowManager());

  ASSERT_TRUE(BorealisWindowManager::IsAnonymousAppId(
      BorealisServiceFactory::GetForProfile(&profile_)
          ->WindowManager()
          .GetShelfAppId(window->window())));

  EXPECT_FALSE(
      BorealisSecurityDelegate::MakeForTesting(&profile_)->CanSelfActivate(
          window->window()));
}

}  // namespace borealis

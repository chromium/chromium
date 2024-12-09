// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  MockGlicKeyedService(content::BrowserContext* browser_context,
                       GlicProfileManager* profile_manager)
      : GlicKeyedService(browser_context, profile_manager) {}
  MOCK_METHOD(void, ClosePanel, (), (override));
};

TEST(GlicProfileManagerTest, OnUILaunching_SameProfile) {
  content::BrowserTaskEnvironment task_environment;
  GlicProfileManager profile_manager;
  TestingProfile profile;
  MockGlicKeyedService service(&profile, &profile_manager);

  profile_manager.OnUILaunching(&service);

  // Opening glic twice for the same profile shouldn't cause it to close.
  EXPECT_CALL(service, ClosePanel()).Times(0);
  profile_manager.OnUILaunching(&service);
}

TEST(GlicProfileManagerTest, OnUILaunching_DifferentProfiles) {
  content::BrowserTaskEnvironment task_environment;
  GlicProfileManager profile_manager;
  TestingProfile profile1;
  TestingProfile profile2;
  MockGlicKeyedService service1(&profile1, &profile_manager);
  MockGlicKeyedService service2(&profile2, &profile_manager);

  profile_manager.OnUILaunching(&service1);

  // Opening glic from a second profile should make the profile manager close
  // the first one.
  EXPECT_CALL(service1, ClosePanel());
  profile_manager.OnUILaunching(&service2);
}

}  // namespace
}  // namespace glic

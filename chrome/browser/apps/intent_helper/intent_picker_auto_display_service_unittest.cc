// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class IntentPickerAutoDisplayServiceTest : public testing::Test {
 public:
  IntentPickerAutoDisplayServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(IntentPickerAutoDisplayServiceTest, GetPlatform) {
  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;
  IntentPickerAutoDisplayService* service =
      IntentPickerAutoDisplayService::Get(&profile);

  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url2));
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform to a value and check value has updated.
  service->UpdatePlatformForTablets(
      url1, IntentPickerAutoDisplayService::Platform::kArc);
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url2));

  // Url with a different host should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform and check value has updated.
  service->UpdatePlatformForTablets(
      url2, IntentPickerAutoDisplayService::Platform::kChrome);
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));

  // Url with a different host should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform and check value has updated.
  service->UpdatePlatformForTablets(
      url3, IntentPickerAutoDisplayService::Platform::kArc);
  // Url with different hosts should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));

  // Url value should be updated.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url3));

  service->UpdatePlatformForTablets(
      url3, IntentPickerAutoDisplayService::Platform::kNone);
  // Url value should be updated.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));
  // Url with different hosts should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayService::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));
}

// Checks that calling IncrementCounter twice for a particular URL causes
// the UI to no longer be auto-displayed for that URL.
TEST_F(IntentPickerAutoDisplayServiceTest, ShouldAutoDisplayUi) {
  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;
  IntentPickerAutoDisplayService* service =
      IntentPickerAutoDisplayService::Get(&profile);

  EXPECT_TRUE(service->ShouldAutoDisplayUi(url1));
  service->IncrementCounter(url1);
  EXPECT_TRUE(service->ShouldAutoDisplayUi(url1));
  service->IncrementCounter(url1);
  EXPECT_FALSE(service->ShouldAutoDisplayUi(url1));

  // Should return false for a different URL on the same host.
  EXPECT_FALSE(service->ShouldAutoDisplayUi(url2));
  // Should return true for a different host.
  EXPECT_TRUE(service->ShouldAutoDisplayUi(url3));
}

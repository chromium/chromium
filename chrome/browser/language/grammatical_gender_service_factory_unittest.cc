// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/grammatical_gender_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/grammatical_gender_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsNull;
using testing::Not;

TEST(GrammaticalGenderServiceFactoryTest, SharedWithIncognito) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  const language::GrammaticalGenderService* const service =
      GrammaticalGenderServiceFactory::GetForProfile(&profile);
  ASSERT_THAT(service, Not(IsNull()));

  Profile* const incognito =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(GrammaticalGenderServiceFactory::GetForProfile(incognito),
              Eq(service));
}

TEST(GrammaticalGenderServiceFactoryTest, NoServiceForGuestProfile) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = builder.Build();
  EXPECT_THAT(
      GrammaticalGenderServiceFactory::GetForProfile(guest_profile.get()),
      IsNull());
}

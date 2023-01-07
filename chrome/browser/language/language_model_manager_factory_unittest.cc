// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_manager_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/language_model_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Eq;
using testing::IsNull;
using testing::Not;

// Check that Incognito language modeling is inherited from the user's profile.
TEST(LanguageModelManagerFactoryTest, SharedWithIncognito) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  const language::LanguageModelManager* const manager =
      LanguageModelManagerFactory::GetForBrowserContext(&profile);
  EXPECT_THAT(manager, Not(IsNull()));

  Profile* const incognito =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(LanguageModelManagerFactory::GetForBrowserContext(incognito),
              Eq(manager));

  // Must wait for task posted in PrepareLanguageModels to complete.
  task_environment.RunUntilIdle();
}

TEST(LanguageModelManagerFactoryTest, GetLanguageModels) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;
  const language::LanguageModelManager* const manager =
      LanguageModelManagerFactory::GetForBrowserContext(&profile);
  EXPECT_THAT(manager, Not(IsNull()));

  // Must wait for task posted in PrepareLanguageModels to complete.
  task_environment.RunUntilIdle();

  // The test manager should be initially populated with a primary model and a
  // ULPLanguageModel.
  EXPECT_THAT(manager->GetPrimaryModel(), Not(IsNull()));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(
      manager->GetLanguageModel(language::LanguageModelManager::ModelType::ULP),
      Not(IsNull()));
#endif
}

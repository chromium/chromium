// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {
namespace {

class SkillRecordingServiceFactoryTest : public testing::Test {
 public:
  SkillRecordingServiceFactoryTest() = default;
  ~SkillRecordingServiceFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SkillRecordingServiceFactoryTest, ReturnsNullForNullProfile) {
  scoped_feature_list_.InitAndEnableFeature(
      browser::context_hub::mojom::kRecordReplay);
  EXPECT_EQ(nullptr, SkillRecordingServiceFactory::GetForProfile(nullptr));
}

TEST_F(SkillRecordingServiceFactoryTest, CreatesServiceWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      browser::context_hub::mojom::kRecordReplay);
  TestingProfile profile;
  EXPECT_NE(nullptr, SkillRecordingServiceFactory::GetForProfile(&profile));
}

TEST_F(SkillRecordingServiceFactoryTest, CreatesNoServiceWithFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      browser::context_hub::mojom::kRecordReplay);
  TestingProfile profile;
  EXPECT_EQ(nullptr, SkillRecordingServiceFactory::GetForProfile(&profile));
}

TEST_F(SkillRecordingServiceFactoryTest,
       DoesNotCreateServiceForIncognitoWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      browser::context_hub::mojom::kRecordReplay);
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_EQ(nullptr, SkillRecordingServiceFactory::GetForProfile(otr_profile));
}

TEST_F(SkillRecordingServiceFactoryTest,
       DoesNotCreateServiceForGuestWithFlagEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      browser::context_hub::mojom::kRecordReplay);
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();

  EXPECT_EQ(nullptr,
            SkillRecordingServiceFactory::GetForProfile(guest_profile.get()));

  Profile* guest_otr_profile = guest_profile->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            SkillRecordingServiceFactory::GetForProfile(guest_otr_profile));
}

}  // namespace
}  // namespace context_hub

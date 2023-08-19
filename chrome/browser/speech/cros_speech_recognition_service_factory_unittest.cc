// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using testing::IsNull;
using testing::Not;

// Verifies that the service factory supports incognito profiles.
TEST(CrosSpeechRecognitionServiceFactoryTest, IncognitoProfile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(ash::features::kOnDeviceSpeechRecognition);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;

  const speech::SpeechRecognitionService* const service =
      CrosSpeechRecognitionServiceFactory::GetForProfile(
          profile.GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_THAT(service, Not(IsNull()));
}

}  // namespace

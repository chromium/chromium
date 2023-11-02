// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_service_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::IsNull;
using testing::Not;

// Verifies that the service factory supports incognito profiles.
TEST(SpeechRecognitionServiceFactoryTest, IncognitoProfile) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile profile;

  const speech::SpeechRecognitionService* const service =
      SpeechRecognitionServiceFactory::GetForProfile(
          profile.GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_THAT(service, Not(IsNull()));
}

}  // namespace

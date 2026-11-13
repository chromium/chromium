// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/audio_survey_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Exactly;
using ::testing::Return;

namespace ash {
namespace {

constexpr AccountId::Literal kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("user@example.com",
                                            GaiaId::Literal("1234567890"));

class MockDelegate : public AudioSurveyHandler::Delegate {
 public:
  MOCK_METHOD(void,
              AddAudioObserver,
              (CrasAudioHandler::AudioObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveAudioObserver,
              (CrasAudioHandler::AudioObserver * observer),
              (override));
  MOCK_METHOD(bool,
              ShouldShowSurvey,
              (CrasAudioHandler::SurveyType type, const AccountId& account_id),
              (const, override));
  MOCK_METHOD(void,
              ShowSurvey,
              (CrasAudioHandler::SurveyType type,
               const CrasAudioHandler::AudioSurveyData& data,
               const AccountId& account_id),
              (override));
};

class AudioSurveyHandlerTest : public testing::Test {
 protected:
  AudioSurveyHandlerTest() {
    delegate_ = nullptr;
    survey_handler_ = nullptr;
    test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(&local_state_);
    CHECK(user_session_test_environment_->AddRegularUser(kAccountId));
    user_session_test_environment_->LogIn(kAccountId);
  }

  void TearDown() override {
    delegate_ = nullptr;
    survey_handler_ = nullptr;
  }

  ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  base::test::ScopedFeatureList features_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<AudioSurveyHandler> survey_handler_;
  raw_ptr<MockDelegate> delegate_ = nullptr;
};

class AudioSurveyHandlerNoSurveyTest : public AudioSurveyHandlerTest {
 protected:
  void SetUp() override {
    features_.InitWithFeatures(
        {}, {kHatsAudioSurvey.feature, kHatsBluetoothAudioSurvey.feature});
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, AddAudioObserver).Times(Exactly(0));
    survey_handler_ = std::make_unique<AudioSurveyHandler>(std::move(delegate));
  }

  void TearDown() override {
    EXPECT_CALL(*delegate_, RemoveAudioObserver).Times(Exactly(0));
    AudioSurveyHandlerTest::TearDown();
  }
};

TEST_F(AudioSurveyHandlerNoSurveyTest, CheckFeature) {}

class AudioSurveyHandlerWithSurveyTest : public AudioSurveyHandlerTest {
 protected:
  void SetUp() override {
    features_.InitWithFeatures(
        {kHatsAudioSurvey.feature, kHatsBluetoothAudioSurvey.feature}, {});
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, AddAudioObserver).Times(Exactly(1));
    survey_handler_ = std::make_unique<AudioSurveyHandler>(std::move(delegate));
  }

  void TearDown() override {
    EXPECT_CALL(*delegate_, RemoveAudioObserver).Times(Exactly(1));
    AudioSurveyHandlerTest::TearDown();
  }
};

TEST_F(AudioSurveyHandlerWithSurveyTest, ShowSurvey) {
  EXPECT_CALL(*delegate_, ShouldShowSurvey).WillOnce(Return(true));
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(1));

  CrasAudioHandler::AudioSurvey survey;
  survey.AddData("key", "value");
  survey_handler_->OnSurveyTriggered(survey);
}

TEST_F(AudioSurveyHandlerWithSurveyTest, NoSurvey) {
  EXPECT_CALL(*delegate_, ShouldShowSurvey).WillOnce(Return(false));
  EXPECT_CALL(*delegate_, ShowSurvey).Times(Exactly(0));

  CrasAudioHandler::AudioSurvey survey;
  survey_handler_->OnSurveyTriggered(survey);
}

}  // namespace
}  // namespace ash

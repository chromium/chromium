// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/babelorca/soda_testing_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/mock_soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

using ::testing::_;

constexpr char kDefaultLanguage[] = "en-US";
constexpr char kOtherLanguage[] = "fr-FR";

// Mocks
class MockBabelOrcaSpeechRecognizerObserver
    : public BabelOrcaSpeechRecognizer::Observer {
 public:
  MockBabelOrcaSpeechRecognizerObserver() = default;
  ~MockBabelOrcaSpeechRecognizerObserver() override = default;

  MOCK_METHOD(void,
              OnTranscriptionResult,
              (const media::SpeechRecognitionResult& result,
               const std::string& source_language),
              (override));
  MOCK_METHOD(void,
              OnLanguageIdentificationEvent,
              (const media::mojom::LanguageIdentificationEventPtr& event),
              (override));
};

class MockSpeechRecognizer : public SpeechRecognizer {
 public:
  // The real SpeechRecognizer constructor is not virtual, so we can't mock it
  // directly. However, we can provide a mock implementation that satisfies the
  // interface for testing purposes. The parameters are ignored in the mock.
  MockSpeechRecognizer(const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
                       Profile* profile,
                       const std::string& device_id,
                       media::mojom::SpeechRecognitionOptionsPtr options)
      : SpeechRecognizer(delegate) {}
  ~MockSpeechRecognizer() override { OnDestruction(); }

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, OnDestruction, ());
};

}  // namespace

class BabelOrcaSpeechRecognizerClientTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    mock_soda_installer_ =
        std::make_unique<testing::NiceMock<speech::MockSodaInstaller>>();
    ON_CALL(*mock_soda_installer_, GetAvailableLanguages)
        .WillByDefault(testing::Return(
            std::vector<std::string>({kDefaultLanguage, kOtherLanguage})));
    mock_soda_installer_->NotifySodaInstalledForTesting();
    mock_soda_installer_->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
    RegisterSodaPrefsForTesting(prefs_.registry());
    prefs_.registry()->RegisterStringPref(
        ash::prefs::kClassManagementToolsAvailabilitySetting, "teacher");
    soda_installer_wrapper_ =
        std::make_unique<SodaInstaller>(&prefs_, &prefs_, kDefaultLanguage);
    recognizer_impl_ = std::make_unique<BabelOrcaSpeechRecognizerClient>(
        /*profile=*/nullptr, soda_installer_wrapper_.get(), kDefaultLanguage,
        kDefaultLanguage);
    recognizer_impl_->SetSpeechRecognizerFactoryForTesting(base::BindRepeating(
        &BabelOrcaSpeechRecognizerClientTest::CreateMockSpeechRecognizer,
        base::Unretained(this)));
    recognizer_impl_->AddObserver(&observer_);
  }

  std::unique_ptr<SpeechRecognizer> CreateMockSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate,
      Profile* profile,
      const std::string& device_id,
      media::mojom::SpeechRecognitionOptionsPtr options) {
    auto mock_recognizer =
        std::make_unique<testing::NiceMock<MockSpeechRecognizer>>(
            delegate, profile, device_id, std::move(options));
    last_mock_speech_recognizer_ = mock_recognizer.get();
    ON_CALL(*mock_recognizer, OnDestruction).WillByDefault([this]() {
      last_mock_speech_recognizer_ = nullptr;
    });
    return mock_recognizer;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<testing::NiceMock<speech::MockSodaInstaller>>
      mock_soda_installer_;
  std::unique_ptr<SodaInstaller> soda_installer_wrapper_;
  std::unique_ptr<BabelOrcaSpeechRecognizerClient> recognizer_impl_;
  testing::NiceMock<MockBabelOrcaSpeechRecognizerObserver> observer_;
  raw_ptr<MockSpeechRecognizer> last_mock_speech_recognizer_ = nullptr;
};

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       SodaInstallationSuccessCreatesRecognizer) {
  recognizer_impl_->Start();
  EXPECT_THAT(last_mock_speech_recognizer_, testing::NotNull());
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       SodaInstallationFailureDoesNotCreateRecognizer) {
  mock_soda_installer_->NotifySodaErrorForTesting();
  recognizer_impl_->Start();
  EXPECT_THAT(last_mock_speech_recognizer_, testing::IsNull());
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       StopBeforeSodaInstallationCompletes) {
  mock_soda_installer_.reset();
  mock_soda_installer_ =
      std::make_unique<testing::NiceMock<speech::MockSodaInstaller>>();
  ON_CALL(*mock_soda_installer_, GetAvailableLanguages)
      .WillByDefault(testing::Return(
          std::vector<std::string>({kDefaultLanguage, kOtherLanguage})));
  recognizer_impl_->Start();
  recognizer_impl_->Stop();
  mock_soda_installer_->NotifySodaInstalledForTesting();
  mock_soda_installer_->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);

  EXPECT_THAT(last_mock_speech_recognizer_, testing::IsNull());
}

TEST_F(BabelOrcaSpeechRecognizerClientTest, OnSpeechResultForwardsToObserver) {
  media::SpeechRecognitionResult result("hello", false);
  EXPECT_CALL(observer_, OnTranscriptionResult(result, kDefaultLanguage))
      .Times(1);
  recognizer_impl_->OnSpeechResult(u"hello", false, result);
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnSpeechResultWithNoResultIsIgnored) {
  EXPECT_CALL(observer_, OnTranscriptionResult(_, _)).Times(0);
  recognizer_impl_->OnSpeechResult(u"hello", false, std::nullopt);
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnLanguageIdentificationEventForwardsToObserver) {
  auto event = media::mojom::LanguageIdentificationEvent::New();
  event->language = kOtherLanguage;
  event->asr_switch_result = media::mojom::AsrSwitchResult::kSwitchSucceeded;
  std::string observed_language;
  media::mojom::AsrSwitchResult observed_asr_switch_result;

  // The observer gets a const ref to the Ptr, so we need to clone it to check.
  EXPECT_CALL(observer_, OnLanguageIdentificationEvent)
      .WillOnce([&observed_language, &observed_asr_switch_result](
                    const media::mojom::LanguageIdentificationEventPtr& event) {
        observed_language = event->language;
        observed_asr_switch_result = event->asr_switch_result.value();
      });
  recognizer_impl_->OnLanguageIdentificationEvent(std::move(event));

  // Check that source language was updated.
  media::SpeechRecognitionResult result("bonjour", false);
  EXPECT_CALL(observer_, OnTranscriptionResult(result, kOtherLanguage))
      .Times(1);
  recognizer_impl_->OnSpeechResult(u"bonjour", false, result);
  EXPECT_THAT(observed_language, testing::StrEq(kOtherLanguage));
  EXPECT_EQ(observed_asr_switch_result,
            media::mojom::AsrSwitchResult::kSwitchSucceeded);
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnLanguageIdentificationEventWithFailedSwitch) {
  auto event = media::mojom::LanguageIdentificationEvent::New();
  event->language = kOtherLanguage;
  event->asr_switch_result = media::mojom::AsrSwitchResult::kSwitchFailed;

  EXPECT_CALL(observer_, OnLanguageIdentificationEvent).Times(0);
  recognizer_impl_->OnLanguageIdentificationEvent(std::move(event));

  // Check that source language was NOT updated.
  media::SpeechRecognitionResult result("hello", false);
  EXPECT_CALL(observer_, OnTranscriptionResult(result, kDefaultLanguage))
      .Times(1);
  recognizer_impl_->OnSpeechResult(u"hello", false, result);
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnSpeechRecognitionStateChangedToReadyStartsRecognizer) {
  recognizer_impl_->Start();

  ASSERT_THAT(last_mock_speech_recognizer_, testing::NotNull());
  EXPECT_CALL(*last_mock_speech_recognizer_, Start()).Times(1);
  recognizer_impl_->OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnSpeechRecognitionStateChangedToErrorResetsRecognizer) {
  recognizer_impl_->Start();
  recognizer_impl_->OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_ERROR);
  EXPECT_THAT(last_mock_speech_recognizer_, testing::IsNull());
}

TEST_F(BabelOrcaSpeechRecognizerClientTest, StopResetsRecognizer) {
  recognizer_impl_->Start();
  recognizer_impl_->Stop();
  EXPECT_THAT(last_mock_speech_recognizer_, testing::IsNull());
}

TEST_F(BabelOrcaSpeechRecognizerClientTest,
       OnSpeechRecognitionStoppedResetsRecognizer) {
  recognizer_impl_->Start();
  recognizer_impl_->OnSpeechRecognitionStopped();
  EXPECT_THAT(last_mock_speech_recognizer_, testing::IsNull());
}

TEST_F(
    BabelOrcaSpeechRecognizerClientTest,
    OnSpeechRecognitionStateChangedToReadyAfterStoppingDoesNotStartRecognizer) {
  recognizer_impl_->Start();

  ASSERT_THAT(last_mock_speech_recognizer_, testing::NotNull());
  EXPECT_CALL(*last_mock_speech_recognizer_, Start()).Times(0);
  recognizer_impl_->OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus::SPEECH_RECOGNITION_STOPPING);
  recognizer_impl_->OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_READY);
}

}  // namespace ash::babelorca

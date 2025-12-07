// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/soda/mock_soda_installer.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {
namespace {

class SpeechRecognitionRecognizerClient
    : public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  SpeechRecognitionRecognizerClient() = default;
  ~SpeechRecognitionRecognizerClient() override = default;

  // media::mojom::SpeechRecognitionRecognizerClient
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}
};

struct CrosSpeechRecognitionServiceTestCase {
  std::string test_name;
  media::mojom::RecognizerClientType client_type;
  bool expected_mask_offensive_words;
};

using CrosSpeechRecognitionServiceTest =
    testing::TestWithParam<CrosSpeechRecognitionServiceTestCase>;

TEST_P(CrosSpeechRecognitionServiceTest, SetMaskOffensiveWords) {
  base::test::ScopedFeatureList features(
      ash::features::kOnDeviceSpeechRecognition);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  testing::NiceMock<speech::MockSodaInstaller> soda_installer;
  base::test::TestFuture<bool> test_future;
  auto test_cb = base::BindLambdaForTesting(
      [&test_future](
          mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
              client,
          media::mojom::SpeechRecognitionOptionsPtr options,
          const base::FilePath& binary_path,
          const base::flat_map<std::string, base::FilePath>& config_paths,
          const std::string& primary_language_name,
          const bool mask_offensive_words)
          -> std::unique_ptr<CrosSpeechRecognitionRecognizerImpl> {
        test_future.GetCallback().Run(mask_offensive_words);
        return std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
            std::move(client), std::move(options), binary_path, config_paths,
            primary_language_name, mask_offensive_words);
      });
  speech::CrosSpeechRecognitionService service(profile.GetOriginalProfile());
  service.SetCreateCrosSpeechRecognitionRecognizerCbForTesting(
      std::move(test_cb));
  mojo::Remote<media::mojom::AudioSourceFetcher> audio_source_fetcher;
  SpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver{&client};

  service.BindAudioSourceFetcher(
      audio_source_fetcher.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver.BindNewPipeAndPassRemote(),
      media::mojom::SpeechRecognitionOptions::New(
          media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/true, "en-US",
          /*is_server_based=*/false, GetParam().client_type),
      base::DoNothing());

  EXPECT_EQ(test_future.Get(), GetParam().expected_mask_offensive_words);
}

INSTANTIATE_TEST_SUITE_P(
    CrosSpeechRecognitionServiceTestSuite,
    CrosSpeechRecognitionServiceTest,
    testing::ValuesIn<CrosSpeechRecognitionServiceTestCase>({
        {"LiveCaption", media::mojom::RecognizerClientType::kLiveCaption,
         false},
        {"Dictation", media::mojom::RecognizerClientType::kDictation, false},
        {"CastModerator", media::mojom::RecognizerClientType::kCastModerator,
         false},
        {"Projector", media::mojom::RecognizerClientType::kProjector, false},
        {"SchoolTools", media::mojom::RecognizerClientType::kSchoolTools, true},
    }),
    [](const testing::TestParamInfo<
        CrosSpeechRecognitionServiceTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace speech

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/soda/constants.h"
#include "components/soda/mock_soda_installer.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
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

bool TryBindRecognizer(
    mojo::Remote<media::mojom::SpeechRecognitionContext>& context,
    media::mojom::RecognizerClientType client_type,
    std::string_view language,
    bool is_server_based = false) {
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  SpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);
  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->enable_formatting = false;
  options->language = std::string(language);
  options->is_server_based = is_server_based;
  options->recognizer_client_type = client_type;

  base::test::TestFuture<bool> future;
  context->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                          client_receiver.BindNewPipeAndPassRemote(),
                          std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
  recognizer.FlushForTesting();
  return recognizer.is_connected();
}

TEST(CrosSpeechRecognitionServiceBindRecognizerTest,
     FailsCleanlyWhenOnDeviceSpeechDisabledOrInstallerMissing) {
  {
    base::test::ScopedFeatureList disabled_features;
    disabled_features.InitAndDisableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    content::BrowserTaskEnvironment task_environment;
    TestingProfile profile;
    testing::NiceMock<speech::MockSodaInstaller> soda_installer;
    speech::CrosSpeechRecognitionService service(profile.GetOriginalProfile());

    mojo::Remote<media::mojom::SpeechRecognitionContext> context;
    service.BindSpeechRecognitionContext(context.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(TryBindRecognizer(
        context, media::mojom::RecognizerClientType::kTtc, "en-US"));
  }

  {
    base::test::ScopedFeatureList enabled_features(
        ash::features::kOnDeviceSpeechRecognition);
    content::BrowserTaskEnvironment task_environment;
    TestingProfile profile;
    speech::CrosSpeechRecognitionService service(profile.GetOriginalProfile());

    mojo::Remote<media::mojom::SpeechRecognitionContext> context;
    service.BindSpeechRecognitionContext(context.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(TryBindRecognizer(
        context, media::mojom::RecognizerClientType::kTtc, "en-US"));
  }
}

TEST(CrosSpeechRecognitionServiceBindRecognizerTest,
     RequiresInstalledSodaPathsAndSupportsRegionalDlcPacks) {
  base::test::ScopedFeatureList enabled_features(
      ash::features::kOnDeviceSpeechRecognition);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  testing::NiceMock<speech::MockSodaInstaller> soda_installer;
  speech::CrosSpeechRecognitionService service(profile.GetOriginalProfile());

  mojo::Remote<media::mojom::SpeechRecognitionContext> context;
  service.BindSpeechRecognitionContext(context.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kTtc, "en-US"));

  ON_CALL(soda_installer, GetSodaBinaryPath())
      .WillByDefault(testing::Return(base::FilePath("/fake/libsoda")));
  soda_installer.NotifySodaInstalledForTesting();

  // Regional ChromeOS DLC installed (`en-GB`, not in desktop component list).
  ON_CALL(soda_installer, GetAvailableLanguages())
      .WillByDefault(testing::Return(std::vector<std::string>{"en-GB"}));
  ON_CALL(soda_installer, GetLanguagePath(std::string_view("en-GB")))
      .WillByDefault(testing::Return(base::FilePath("/fake/en-gb")));
  soda_installer.NotifySodaInstalledForTesting(speech::LanguageCode::kEnGb);

  EXPECT_TRUE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kLiveCaption, "en-GB"));
  EXPECT_TRUE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kTtc, "en-GB"));
  EXPECT_TRUE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kTtc, "en-AU"));
  EXPECT_FALSE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kLiveCaption, "en-AU"));

  // Desktop-mapped language pack (`en-US`) also supports subtag fallback.
  ON_CALL(soda_installer, GetLanguagePath(std::string_view("en-US")))
      .WillByDefault(testing::Return(base::FilePath("/fake/en-us")));
  soda_installer.NotifySodaInstalledForTesting(speech::LanguageCode::kEnUs);
  EXPECT_TRUE(TryBindRecognizer(
      context, media::mojom::RecognizerClientType::kTtc, "en-CA"));
}

TEST(CrosSpeechRecognitionServiceBindRecognizerTest,
     RejectsDisallowedClientTypesAndServerBasedOptions) {
  base::test::ScopedFeatureList enabled_features(
      ash::features::kOnDeviceSpeechRecognition);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  testing::NiceMock<speech::MockSodaInstaller> soda_installer;
  speech::CrosSpeechRecognitionService service(profile.GetOriginalProfile());

  {
    mojo::Remote<media::mojom::SpeechRecognitionContext> context;
    service.BindSpeechRecognitionContext(context.BindNewPipeAndPassReceiver());
    mojo::test::BadMessageObserver bad_message;
    mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
    SpeechRecognitionRecognizerClient client;
    mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
        client_receiver(&client);
    auto options = media::mojom::SpeechRecognitionOptions::New(
        media::mojom::SpeechRecognitionMode::kCaption,
        /*enable_formatting=*/false, "en-US", /*is_server_based=*/false,
        media::mojom::RecognizerClientType::kDictation);
    context->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), base::DoNothing());
    EXPECT_EQ("Invalid SpeechRecognitionOptions provided",
              bad_message.WaitForBadMessage());
  }

  {
    mojo::Remote<media::mojom::SpeechRecognitionContext> context;
    service.BindSpeechRecognitionContext(context.BindNewPipeAndPassReceiver());
    mojo::test::BadMessageObserver bad_message;
    mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
    SpeechRecognitionRecognizerClient client;
    mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
        client_receiver(&client);
    auto options = media::mojom::SpeechRecognitionOptions::New(
        media::mojom::SpeechRecognitionMode::kCaption,
        /*enable_formatting=*/false, "en-US", /*is_server_based=*/true,
        media::mojom::RecognizerClientType::kTtc);
    context->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), base::DoNothing());
    EXPECT_EQ("Invalid SpeechRecognitionOptions provided",
              bad_message.WaitForBadMessage());
  }
}

}  // namespace
}  // namespace speech

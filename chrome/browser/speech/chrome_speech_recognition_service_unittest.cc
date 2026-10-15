// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_installer.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS)

namespace speech {

namespace {

class FakeSpeechRecognitionRecognizerClient
    : public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  FakeSpeechRecognitionRecognizerClient() = default;
  ~FakeSpeechRecognitionRecognizerClient() override = default;

  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}
};

class FakeSpeechRecognitionBrowserObserver
    : public media::mojom::SpeechRecognitionBrowserObserver {
 public:
  FakeSpeechRecognitionBrowserObserver() = default;
  ~FakeSpeechRecognitionBrowserObserver() override = default;

  void SpeechRecognitionAvailabilityChanged(
      bool is_speech_recognition_available) override {
    availability_changed_count_++;
    last_availability_ = is_speech_recognition_available;
  }
  void SpeechRecognitionLanguageChanged(const std::string& language) override {}
  void SpeechRecognitionMaskOffensiveWordsChanged(
      bool mask_offensive_words) override {}

  int availability_changed_count() const { return availability_changed_count_; }
  std::optional<bool> last_availability() const { return last_availability_; }

 private:
  int availability_changed_count_ = 0;
  std::optional<bool> last_availability_;
};

}  // namespace

class ChromeSpeechRecognitionServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    if (!TestingBrowserProcess::GetGlobal()
             ->GetTestingLocalState()
             ->FindPreference(prefs::kSodaBinaryPath)) {
      SodaInstaller::RegisterLocalStatePrefs(TestingBrowserProcess::GetGlobal()
                                                 ->GetTestingLocalState()
                                                 ->registry());
    }
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<ChromeSpeechRecognitionService>(profile_.get());
  }

  void TearDown() override {
    service_.reset();
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaRegisteredLanguagePacks);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaEnUsConfigPath);
  }

  ChromeSpeechRecognitionService* service() { return service_.get(); }
  TestingProfile* profile() { return profile_.get(); }

  base::flat_map<std::string, base::FilePath> GetSodaConfigPaths() {
    return service_->GetSodaConfigPaths();
  }

  size_t GetOnDeviceModelRecognizerCount() {
    return service_->on_device_recognizers_.size();
  }

  bool HasModelBrokerClient() {
    return service_->model_broker_client_ != nullptr;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeSpeechRecognitionService> service_;
};

TEST_F(ChromeSpeechRecognitionServiceTest,
       GetSodaConfigPathsExcludesSmallExpertModel) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  base::ListValue registered_languages;
  registered_languages.Append("en-US");
  registered_languages.Append("speech_recognition_small_expert_model");
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      prefs::kSodaRegisteredLanguagePacks, std::move(registered_languages));

  base::FilePath en_us_path(FILE_PATH_LITERAL("/path/to/en_us"));
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetFilePath(
      prefs::kSodaEnUsConfigPath, en_us_path);

  base::flat_map<std::string, base::FilePath> config_paths =
      GetSodaConfigPaths();
  EXPECT_TRUE(config_paths.contains("en-US"));
  EXPECT_EQ(en_us_path, config_paths["en-US"]);
  EXPECT_FALSE(config_paths.contains("speech_recognition_small_expert_model"));
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       RoutingToSodaWhenFeatureFlagDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "en-US";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest, NonEnglishLanguageRoutesToSoda) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "es-ES";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       NoRoutingFallbackToSodaWhenModelBrokerUnavailable) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "en-US";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       RoutingToSodaWhenNonLiveCaptionClientType) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kDictation;
  options->language = "en-US";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       RoutingToSodaWhenServerBasedRequested) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->is_server_based = true;
  options->language = "en-US";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest, NonEnglishPrefLanguageRoutesToSoda) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "fr-FR");

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = std::nullopt;

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       RoutingToSodaWhenLanguageUnsetAndFeatureFlagDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = std::nullopt;

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       LanguageOptionsPropagatedWhenUnsetInOptions) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = std::nullopt;

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       NoRoutingFallbackToSodaWhenSmallExpertModelNotInstalled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);

  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "en-US";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       CloseOnDeviceModelServiceHandlesOnStateChange) {
  service()->OnSpeechRecognitionSmallExpertModelStateChanged(
      SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState::kDownloading);
  task_environment_.RunUntilIdle();

  service()->OnSpeechRecognitionSmallExpertModelStateChanged(
      SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState::kNotInstalled);
  task_environment_.RunUntilIdle();

  service()->OnSpeechRecognitionSmallExpertModelStateChanged(
      SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState::kError);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetOnDeviceModelRecognizerCount(), 0u);
  EXPECT_FALSE(HasModelBrokerClient());

  service()->OnSpeechRecognitionSmallExpertModelStateChanged(
      SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState::kErrorCorruptPersistent);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetOnDeviceModelRecognizerCount(), 0u);
  EXPECT_FALSE(HasModelBrokerClient());
}

class SpeechRecognitionClientBrowserInterfaceTest : public testing::Test {
 protected:
  void SetUp() override {
    if (!TestingBrowserProcess::GetGlobal()
             ->GetTestingLocalState()
             ->FindPreference(prefs::kSodaBinaryPath)) {
      SodaInstaller::RegisterLocalStatePrefs(TestingBrowserProcess::GetGlobal()
                                                 ->GetTestingLocalState()
                                                 ->registry());
    }
    if (!TestingBrowserProcess::GetGlobal()
             ->GetTestingLocalState()
             ->FindPreference(prefs::kSpeechRecognitionSmallExpertModelPath)) {
      SpeechRecognitionSmallExpertModelInstaller::RegisterLocalStatePrefs(
          TestingBrowserProcess::GetGlobal()
              ->GetTestingLocalState()
              ->registry());
    }
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        captions::LiveCaptionControllerFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return nullptr;
            }));
    profile_ = builder.Build();
  }

  void TearDown() override {
    browser_interface_.reset();
    profile_.reset();
    TestingBrowserProcess::GetGlobal()
        ->SetSpeechRecognitionSmallExpertModelInstaller(nullptr);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaRegisteredLanguagePacks);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaEnUsConfigPath);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSpeechRecognitionSmallExpertModelPath);
  }

  void CreateInterface() {
    browser_interface_ =
        std::make_unique<SpeechRecognitionClientBrowserInterface>(
            profile_.get());
    browser_interface_->BindSpeechRecognitionBrowserObserver(
        observer_receiver_.BindNewPipeAndPassRemote());
    task_environment_.RunUntilIdle();
  }

  TestingProfile* profile() { return profile_.get(); }
  FakeSpeechRecognitionBrowserObserver* observer() { return &observer_; }
  SpeechRecognitionClientBrowserInterface* browser_interface() {
    return browser_interface_.get();
  }
  MockSodaInstaller& mock_soda() { return mock_soda_; }

  content::BrowserTaskEnvironment task_environment_;
  testing::NiceMock<MockSodaInstaller> mock_soda_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SpeechRecognitionClientBrowserInterface> browser_interface_;
  FakeSpeechRecognitionBrowserObserver observer_;
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      observer_receiver_{&observer_};
};

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       SodaInstalledEnablesLiveCaptionWhenFeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  CreateInterface();
  EXPECT_FALSE(observer()->last_availability().value_or(false));

  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kNone);
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer()->last_availability().value_or(false));
}

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       SodaNotUsedWhenSmallExpertModelEnabledButNotInstalled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  auto installer =
      std::make_unique<SpeechRecognitionSmallExpertModelInstaller>();
  TestingBrowserProcess::GetGlobal()
      ->SetSpeechRecognitionSmallExpertModelInstaller(std::move(installer));

  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kNone);
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  CreateInterface();
  // Small Expert Model is NOT installed. No SODA fallback when feature is
  // enabled.
  EXPECT_FALSE(observer()->last_availability().value_or(true));
}

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       SmallExpertModelInstalledEnablesLiveCaption) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  auto installer =
      std::make_unique<SpeechRecognitionSmallExpertModelInstaller>();
  auto* installer_ptr = installer.get();
  TestingBrowserProcess::GetGlobal()
      ->SetSpeechRecognitionSmallExpertModelInstaller(std::move(installer));

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  CreateInterface();
  EXPECT_FALSE(observer()->last_availability().value_or(false));

  installer_ptr->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer()->last_availability().value_or(false));
}

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       NonEnglishUsesSmallExpertModelWhenEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  auto installer =
      std::make_unique<SpeechRecognitionSmallExpertModelInstaller>();
  auto* installer_ptr = installer.get();
  TestingBrowserProcess::GetGlobal()
      ->SetSpeechRecognitionSmallExpertModelInstaller(std::move(installer));

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "es-ES");

  CreateInterface();
  EXPECT_FALSE(observer()->last_availability().value_or(false));

  // Installing Small Expert Model enables non-English Live Caption.
  installer_ptr->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer()->last_availability().value_or(false));
}

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       SmallExpertModelErrorDoesNotFallBackToSoda) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      media::kLiveCaptionSpeechRecognitionSmallExpertModel);

  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kNone);
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);

  auto installer =
      std::make_unique<SpeechRecognitionSmallExpertModelInstaller>();
  auto* installer_ptr = installer.get();
  TestingBrowserProcess::GetGlobal()
      ->SetSpeechRecognitionSmallExpertModelInstaller(std::move(installer));

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  CreateInterface();
  EXPECT_FALSE(observer()->last_availability().value_or(true));

  installer_ptr->NotifySpeechRecognitionSmallExpertModelInstalledForTesting();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer()->last_availability().value_or(false));

  installer_ptr->NotifySpeechRecognitionSmallExpertModelErrorForTesting();
  task_environment_.RunUntilIdle();
  // SODA is installed, but no fallback when feature is enabled.
  EXPECT_FALSE(observer()->last_availability().value_or(true));
}

TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       LanguageChangeUpdatesAvailability) {
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kNone);
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);

  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  CreateInterface();
  EXPECT_TRUE(observer()->last_availability().value_or(false));

  // Change to language not installed (es-ES) -> becomes unavailable.
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "es-ES");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer()->last_availability().value_or(true));

  // Install es-ES -> becomes available.
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEsEs);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer()->last_availability().value_or(false));
}

}  // namespace speech

#endif  // !BUILDFLAG(IS_CHROMEOS)

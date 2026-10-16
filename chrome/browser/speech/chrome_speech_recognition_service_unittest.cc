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
#include "base/test/run_until.h"
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
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/testing_pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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

class TestCaptionListener : public captions::CaptionControllerBase::Listener {
 public:
  explicit TestCaptionListener(content::GlobalRenderFrameHostId excluded_rfh_id)
      : excluded_rfh_id_(excluded_rfh_id) {}
  ~TestCaptionListener() override = default;

  bool OnTranscription(content::RenderFrameHost*,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult&) override {
    return true;
  }
  void OnAudioStreamEnd(content::RenderFrameHost*,
                        captions::CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost*,
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}
  bool IsTranscriptionAllowed(content::RenderFrameHost* rfh) override {
    return rfh && rfh->GetGlobalId() != excluded_rfh_id_;
  }

 private:
  content::GlobalRenderFrameHostId excluded_rfh_id_;
};

class StubCaptionBubbleController : public captions::CaptionBubbleController {
 public:
  StubCaptionBubbleController() = default;
  ~StubCaptionBubbleController() override = default;

  bool OnTranscription(content::RenderFrameHost*,
                       captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult&) override {
    return true;
  }
  void OnAudioStreamEnd(content::RenderFrameHost*,
                        captions::CaptionBubbleContext*) override {}
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost*,
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}
  void OnError(captions::CaptionBubbleContext*,
               captions::CaptionBubbleErrorType,
               captions::OnErrorClickedCallback,
               captions::OnDoNotShowAgainClickedCallback) override {}
  void UpdateCaptionStyle(std::optional<ui::CaptionStyle>) override {}
  bool IsWidgetVisibleForTesting() override { return false; }
  bool IsGenericErrorMessageVisibleForTesting() override { return false; }
  std::string GetBubbleLabelTextForTesting() override { return ""; }
  void CloseActiveModelForTesting() override {}
};

class TestCaptionControllerDelegate
    : public captions::CaptionControllerBase::Delegate {
 public:
  TestCaptionControllerDelegate() = default;
  ~TestCaptionControllerDelegate() override = default;

  std::unique_ptr<captions::CaptionBubbleController>
  CreateCaptionBubbleController(
      captions::CaptionBubbleSettings*,
      const std::string&,
      std::unique_ptr<captions::TranslationViewWrapperBase>) override {
    return std::make_unique<StubCaptionBubbleController>();
  }
  void AddCaptionStyleObserver(ui::NativeThemeObserver*) override {}
  void RemoveCaptionStyleObserver(ui::NativeThemeObserver*) override {}
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
        observer_receiver_.BindNewPipeAndPassRemote(),
        content::GlobalRenderFrameHostId());
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

// Headless caption with SODA requires on-device speech recognition support,
// which is not available on non-x86 Linux.
#if !BUILDFLAG(IS_LINUX) || defined(ARCH_CPU_X86_FAMILY)
TEST_F(SpeechRecognitionClientBrowserInterfaceTest,
       HeadlessCaptionChecksIsTranscriptionAllowedPerFrame) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{media::kHeadlessLiveCaption},
      /*disabled_features=*/{
          media::kLiveCaptionSpeechRecognitionSmallExpertModel});

  content::RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<content::WebContents> allowed_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::WebContents> excluded_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::RenderFrameHost* allowed_rfh =
      allowed_web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* excluded_rfh =
      excluded_web_contents->GetPrimaryMainFrame();

  captions::LiveCaptionControllerFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting([&](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<captions::LiveCaptionController>(
            profile()->GetPrefs(),
            TestingBrowserProcess::GetGlobal()->GetTestingLocalState(), "en-US",
            context, std::make_unique<TestCaptionControllerDelegate>());
      }));
  auto* controller =
      captions::LiveCaptionControllerFactory::GetForProfile(profile());
  controller->AddListener(
      std::make_unique<TestCaptionListener>(excluded_rfh->GetGlobalId()));

  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kNone);
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);
  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");

  browser_interface_ =
      std::make_unique<SpeechRecognitionClientBrowserInterface>(profile());

  mojo::Remote<media::mojom::SpeechRecognitionClientBrowserInterface>
      allowed_remote;
  browser_interface_->BindReceiver(allowed_remote.BindNewPipeAndPassReceiver(),
                                   allowed_rfh->GetGlobalId());
  FakeSpeechRecognitionBrowserObserver allowed_observer;
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      allowed_observer_receiver(&allowed_observer);
  allowed_remote->BindSpeechRecognitionBrowserObserver(
      allowed_observer_receiver.BindNewPipeAndPassRemote());

  mojo::Remote<media::mojom::SpeechRecognitionClientBrowserInterface>
      excluded_remote;
  browser_interface_->BindReceiver(excluded_remote.BindNewPipeAndPassReceiver(),
                                   excluded_rfh->GetGlobalId());
  FakeSpeechRecognitionBrowserObserver excluded_observer;
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      excluded_observer_receiver(&excluded_observer);
  excluded_remote->BindSpeechRecognitionBrowserObserver(
      excluded_observer_receiver.BindNewPipeAndPassRemote());

  FakeSpeechRecognitionBrowserObserver no_frame_observer;
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      no_frame_observer_receiver(&no_frame_observer);
  browser_interface_->BindSpeechRecognitionBrowserObserver(
      no_frame_observer_receiver.BindNewPipeAndPassRemote(),
      content::GlobalRenderFrameHostId());

  FakeSpeechRecognitionBrowserObserver destroyed_frame_observer;
  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      destroyed_frame_observer_receiver(&destroyed_frame_observer);
  browser_interface_->BindSpeechRecognitionBrowserObserver(
      destroyed_frame_observer_receiver.BindNewPipeAndPassRemote(),
      content::GlobalRenderFrameHostId(99999, 99999));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return allowed_observer.last_availability().has_value() &&
           excluded_observer.last_availability().has_value() &&
           no_frame_observer.last_availability().has_value() &&
           destroyed_frame_observer.last_availability().has_value();
  }));
  EXPECT_TRUE(allowed_observer.last_availability().value_or(false));
  EXPECT_FALSE(excluded_observer.last_availability().value_or(true));
  EXPECT_FALSE(no_frame_observer.last_availability().value_or(true));
  EXPECT_FALSE(destroyed_frame_observer.last_availability().value_or(true));

  // Even if the caption bubble UI listener is added while in headless mode
  // (e.g. via OnError() -> CreateUI()), the excluded frame should still be
  // blocked.
  controller->create_ui_for_testing();
  ASSERT_NE(controller->caption_bubble_controller_for_testing(), nullptr);
  EXPECT_TRUE(controller->IsTranscriptionAllowed(allowed_rfh));
  EXPECT_FALSE(controller->IsTranscriptionAllowed(excluded_rfh));
  int prev_excluded_count = excluded_observer.availability_changed_count();
  mock_soda().NotifySodaInstalledForTesting(LanguageCode::kEnUs);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return excluded_observer.availability_changed_count() > prev_excluded_count;
  }));
  EXPECT_TRUE(allowed_observer.last_availability().value_or(false));
  EXPECT_FALSE(excluded_observer.last_availability().value_or(true));

  // Enabling regular Live Caption should allow transcription for all observers.
  profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return allowed_observer.last_availability().value_or(false) &&
           excluded_observer.last_availability().value_or(false) &&
           no_frame_observer.last_availability().value_or(false) &&
           destroyed_frame_observer.last_availability().value_or(false);
  }));
}
#endif  // !BUILDFLAG(IS_LINUX) || defined(ARCH_CPU_X86_FAMILY)

}  // namespace speech

#endif  // !BUILDFLAG(IS_CHROMEOS)

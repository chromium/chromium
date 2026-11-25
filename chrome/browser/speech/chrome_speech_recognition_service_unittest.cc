// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_service.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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
#include "components/soda/pref_names.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognition_recognition_context.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
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

class StubSpeechRecognitionRecognizer
    : public media::mojom::SpeechRecognitionRecognizer {
 public:
  StubSpeechRecognitionRecognizer() = default;
  ~StubSpeechRecognitionRecognizer() override = default;

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer,
      std::optional<base::TimeDelta> media_start_pts) override {}
  void MarkDone() override {}
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override {}
};

class StubSpeechRecognitionSession
    : public media::mojom::SpeechRecognitionSession,
      public media::mojom::SpeechRecognitionAudioForwarder {
 public:
  explicit StubSpeechRecognitionSession(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder)
      : audio_forwarder_(this, std::move(audio_forwarder)) {}
  ~StubSpeechRecognitionSession() override = default;

  // media::mojom::SpeechRecognitionSession:
  void Abort() override {}
  void StopCapture() override {}
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override {}

  // media::mojom::SpeechRecognitionAudioForwarder:
  void AddAudioFromRenderer(media::mojom::AudioDataS16Ptr buffer) override {}

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionAudioForwarder>
      audio_forwarder_;
};

class FakeUtilitySpeechRecognitionService
    : public media::mojom::SpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext {
 public:
  explicit FakeUtilitySpeechRecognitionService(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~FakeUtilitySpeechRecognitionService() override = default;

  // media::mojom::SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> context)
      override {
    context_bind_count_++;
    speech_recognition_contexts_.Add(this, std::move(context));
  }
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          context) override {}
  void SetSodaPaths(
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name) override {
    last_binary_path_ = binary_path;
    last_config_paths_ = config_paths;
    last_primary_language_name_ = primary_language_name;
  }
  void SetSodaParams(bool mask_offensive_words) override {}
  void SetSodaConfigPaths(const base::flat_map<std::string, base::FilePath>&
                              config_paths) override {}

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override {
    recognizers_.Add(std::make_unique<StubSpeechRecognitionRecognizer>(),
                     std::move(receiver));
    std::move(callback).Run(/*is_multichannel_supported=*/false);
  }
  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override {
    sessions_.Add(std::make_unique<StubSpeechRecognitionSession>(
                      std::move(audio_forwarder)),
                  std::move(session_receiver));
  }

  int context_bind_count() const { return context_bind_count_; }
  size_t recognizer_count() const { return recognizers_.size(); }
  size_t session_count() const { return sessions_.size(); }
  const base::FilePath& last_binary_path() const { return last_binary_path_; }
  const base::flat_map<std::string, base::FilePath>& last_config_paths() const {
    return last_config_paths_;
  }
  const std::string& last_primary_language_name() const {
    return last_primary_language_name_;
  }

 private:
  int context_bind_count_ = 0;
  base::FilePath last_binary_path_;
  base::flat_map<std::string, base::FilePath> last_config_paths_;
  std::string last_primary_language_name_;
  mojo::Receiver<media::mojom::SpeechRecognitionService> receiver_;
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  mojo::UniqueReceiverSet<media::mojom::SpeechRecognitionRecognizer>
      recognizers_;
  mojo::UniqueReceiverSet<media::mojom::SpeechRecognitionSession> sessions_;
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
        prefs::kSodaBinaryPath);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaRegisteredLanguagePacks);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaEnUsConfigPath);
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->ClearPref(
        prefs::kSodaFrFrConfigPath);
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

  void BindFakeUtilityService(
      mojo::PendingRemote<media::mojom::SpeechRecognitionService> remote) {
    service_->BindServiceRemote(std::move(remote));
  }

  void SetLaunchServiceCallback(
      ChromeSpeechRecognitionService::LaunchServiceCallback callback) {
    service_->launch_service_callback_for_testing_ = std::move(callback);
  }

  void ConfigureInstalledEnUsSoda(const base::FilePath& binary_path,
                                  const base::FilePath& en_us_path) {
    auto* local_state =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
    local_state->SetFilePath(prefs::kSodaBinaryPath, binary_path);
    local_state->SetFilePath(prefs::kSodaEnUsConfigPath, en_us_path);
    base::ListValue registered_languages;
    registered_languages.Append("en-US");
    local_state->SetList(prefs::kSodaRegisteredLanguagePacks,
                         std::move(registered_languages));
  }

  bool IsSpeechRecognitionServiceBound() const {
    return service_->speech_recognition_service_.is_bound();
  }

  bool IsSpeechRecognitionContextRemoteBound() const {
    return service_->speech_recognition_context_remote_.is_bound();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(ChromeSpeechRecognitionServiceTest,
       ServiceResetsAfterIdleTimeoutWhenRecognizerDisconnects) {
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  mojo::Remote<media::mojom::SpeechRecognitionContext> client_context;
  service()->BindSpeechRecognitionContext(
      client_context.BindNewPipeAndPassReceiver());

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
  client_context->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                                 client_receiver.BindNewPipeAndPassRemote(),
                                 std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // While the recognizer remains connected, neither remote should time out.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // Disconnecting the recognizer should immediately reset
  // `speech_recognition_context_remote_` (even while `client_context` remains
  // bound to the browser service) and start the 5-second idle timeout on
  // `speech_recognition_service_`.
  recognizer.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       ServiceStaysAliveAndRebindsContextIfNewRecognizerStartsBeforeTimeout) {
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  auto bind_recognizer =
      [&](mojo::Remote<media::mojom::SpeechRecognitionRecognizer>& recognizer,
          mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>&
              client_receiver) {
        auto options = media::mojom::SpeechRecognitionOptions::New();
        options->recognition_mode =
            media::mojom::SpeechRecognitionMode::kCaption;
        options->recognizer_client_type =
            media::mojom::RecognizerClientType::kLiveCaption;
        options->language = "en-US";

        base::test::TestFuture<bool> future;
        service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                                  client_receiver.BindNewPipeAndPassRemote(),
                                  std::move(options), future.GetCallback());
        EXPECT_FALSE(future.Get());
      };

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer1;
  FakeSpeechRecognitionRecognizerClient client1;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver1(&client1);
  bind_recognizer(recognizer1, client_receiver1);
  EXPECT_EQ(fake_service.context_bind_count(), 1);

  recognizer1.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  // Start a second recognizer 3 seconds into the 5-second idle window.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer2;
  FakeSpeechRecognitionRecognizerClient client2;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver2(&client2);
  bind_recognizer(recognizer2, client_receiver2);
  EXPECT_EQ(fake_service.context_bind_count(), 2);
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // The pending 5-second process idle timer must be canceled while
  // `recognizer2` is active.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  recognizer2.reset();
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       ContextStaysBoundUntilLastConcurrentRecognizerDisconnects) {
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  auto bind_recognizer =
      [&](mojo::Remote<media::mojom::SpeechRecognitionRecognizer>& recognizer,
          mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>&
              client_receiver) {
        auto options = media::mojom::SpeechRecognitionOptions::New();
        options->recognition_mode =
            media::mojom::SpeechRecognitionMode::kCaption;
        options->recognizer_client_type =
            media::mojom::RecognizerClientType::kLiveCaption;
        options->language = "en-US";

        base::test::TestFuture<bool> future;
        service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                                  client_receiver.BindNewPipeAndPassRemote(),
                                  std::move(options), future.GetCallback());
        EXPECT_FALSE(future.Get());
      };

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer1;
  FakeSpeechRecognitionRecognizerClient client1;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver1(&client1);
  bind_recognizer(recognizer1, client_receiver1);

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer2;
  FakeSpeechRecognitionRecognizerClient client2;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver2(&client2);
  bind_recognizer(recognizer2, client_receiver2);

  // Both recognizers multiplex over the same
  // `speech_recognition_context_remote_`.
  EXPECT_EQ(fake_service.context_bind_count(), 1);
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // Disconnecting one recognizer while another remains active must not trigger
  // the zero-duration context idle timeout or the 5-second service timeout.
  recognizer1.reset();
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  // Disconnecting the last recognizer resets the context remote immediately and
  // the service remote after 5 seconds.
  recognizer2.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       ServiceResetsAfterIdleTimeoutWhenWebSpeechSessionDisconnects) {
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  mojo::Remote<media::mojom::SpeechRecognitionSession> session;
  mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
      session_client;
  mojo::PendingReceiver<media::mojom::SpeechRecognitionSessionClient>
      session_client_receiver = session_client.InitWithNewPipeAndPassReceiver();
  mojo::Remote<media::mojom::SpeechRecognitionAudioForwarder> audio_forwarder;

  // Match `SpeechRecognitionManagerImpl`, which binds a temporary context
  // remote and drops it immediately after calling `BindWebSpeechRecognizer()`.
  {
    mojo::Remote<media::mojom::SpeechRecognitionContext> temp_context;
    service()->BindSpeechRecognitionContext(
        temp_context.BindNewPipeAndPassReceiver());

    auto options = media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->language = "en-US";

    temp_context->BindWebSpeechRecognizer(
        session.BindNewPipeAndPassReceiver(), std::move(session_client),
        audio_forwarder.BindNewPipeAndPassReceiver(), /*channel_count=*/1,
        /*sample_rate=*/16000, std::move(options), /*continuous=*/true);
    temp_context.FlushForTesting();
  }

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_service.session_count(), 1u);
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // Even though `temp_context` was dropped, active Web Speech pipes keep the
  // utility service alive.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_TRUE(IsSpeechRecognitionContextRemoteBound());

  // Resetting `session` destroys the utility-side `SodaSpeechRecognizerImpl`
  // (including its owned `audio_forwarder_` receiver, even while the renderer's
  // `audio_forwarder` remote awaits GC), allowing the context remote to reset
  // immediately and the service remote to reset after 5 seconds.
  session.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(IsSpeechRecognitionContextRemoteBound());
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       BindWebSpeechRecognizerWithUnsupportedLiveCaptionPrefLaunchesSoda) {
  base::HistogramTester histogram_tester;
  // "sv-SE" is supported on ChromeOS only and can arrive on desktop profiles
  // via Chrome Sync on `prefs::kLiveCaptionLanguageCode`.
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "sv-SE");
  const base::FilePath binary_path(FILE_PATH_LITERAL("/path/to/soda"));
  const base::FilePath en_us_path(FILE_PATH_LITERAL("/path/to/en_us"));
  ConfigureInstalledEnUsSoda(binary_path, en_us_path);

  std::unique_ptr<FakeUtilitySpeechRecognitionService> fake_service;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>
              receiver) {
        fake_service = std::make_unique<FakeUtilitySpeechRecognitionService>(
            std::move(receiver));
      }));

  mojo::Remote<media::mojom::SpeechRecognitionContext> client_context;
  service()->BindSpeechRecognitionContext(
      client_context.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::SpeechRecognitionSession> session;
  mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
      session_client;
  mojo::PendingReceiver<media::mojom::SpeechRecognitionSessionClient>
      session_client_receiver = session_client.InitWithNewPipeAndPassReceiver();
  mojo::Remote<media::mojom::SpeechRecognitionAudioForwarder> audio_forwarder;

  // Regional BCP-47 tags like "en-GB" match "en-US" by language subtag.
  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "en-GB";

  client_context->BindWebSpeechRecognizer(
      session.BindNewPipeAndPassReceiver(), std::move(session_client),
      audio_forwarder.BindNewPipeAndPassReceiver(), /*channel_count=*/1,
      /*sample_rate=*/48000, std::move(options), /*continuous=*/true);
  client_context.FlushForTesting();
  session.FlushForTesting();

  ASSERT_NE(fake_service, nullptr);
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_EQ(fake_service->session_count(), 1u);
  EXPECT_EQ(fake_service->last_binary_path(), binary_path);
  EXPECT_EQ(fake_service->last_primary_language_name(), "en-US");
  EXPECT_EQ(fake_service->last_config_paths().size(), 1u);
  EXPECT_FALSE(fake_service->last_config_paths().contains("sv-SE"));
  histogram_tester.ExpectUniqueSample("Accessibility.LiveCaption.SodaLanguage",
                                      LanguageCode::kEnUs, 1);
  // Must not overwrite the synced `kLiveCaptionLanguageCode` preference.
  EXPECT_EQ(profile()->GetPrefs()->GetString(prefs::kLiveCaptionLanguageCode),
            "sv-SE");
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       BindWebSpeechRecognizerWithUnsupportedLanguageDoesNotCrash) {
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "sv-SE");
  const base::FilePath binary_path(FILE_PATH_LITERAL("/path/to/soda"));
  const base::FilePath en_us_path(FILE_PATH_LITERAL("/path/to/en_us"));
  ConfigureInstalledEnUsSoda(binary_path, en_us_path);

  bool launch_called = false;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>) {
        launch_called = true;
      }));

  mojo::Remote<media::mojom::SpeechRecognitionContext> client_context;
  service()->BindSpeechRecognitionContext(
      client_context.BindNewPipeAndPassReceiver());

  for (const std::optional<std::string>& language :
       {std::optional<std::string>(), std::optional<std::string>("sv-SE")}) {
    mojo::Remote<media::mojom::SpeechRecognitionSession> session;
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client;
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSessionClient>
        session_client_receiver =
            session_client.InitWithNewPipeAndPassReceiver();
    mojo::Remote<media::mojom::SpeechRecognitionAudioForwarder> audio_forwarder;

    auto options = media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->language = language;

    client_context->BindWebSpeechRecognizer(
        session.BindNewPipeAndPassReceiver(), std::move(session_client),
        audio_forwarder.BindNewPipeAndPassReceiver(), /*channel_count=*/1,
        /*sample_rate=*/48000, std::move(options), /*continuous=*/true);
    client_context.FlushForTesting();
    session.FlushForTesting();
    EXPECT_FALSE(session.is_connected());
  }

  EXPECT_FALSE(launch_called);
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());

  // Even when the utility service is already running, unsupported Web Speech
  // languages must be rejected before forwarding to the utility context.
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  for (const std::optional<std::string>& language :
       {std::optional<std::string>(), std::optional<std::string>("sv-SE")}) {
    mojo::Remote<media::mojom::SpeechRecognitionSession> session;
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client;
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSessionClient>
        session_client_receiver =
            session_client.InitWithNewPipeAndPassReceiver();
    mojo::Remote<media::mojom::SpeechRecognitionAudioForwarder> audio_forwarder;

    auto options = media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->language = language;

    client_context->BindWebSpeechRecognizer(
        session.BindNewPipeAndPassReceiver(), std::move(session_client),
        audio_forwarder.BindNewPipeAndPassReceiver(), /*channel_count=*/1,
        /*sample_rate=*/48000, std::move(options), /*continuous=*/true);
    client_context.FlushForTesting();
    session.FlushForTesting();
    EXPECT_FALSE(session.is_connected());
  }
  EXPECT_EQ(fake_service.session_count(), 0u);
  EXPECT_EQ(profile()->GetPrefs()->GetString(prefs::kLiveCaptionLanguageCode),
            "sv-SE");
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       BindRecognizerWithUnsupportedLiveCaptionPrefAndSupportedOption) {
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "sv-SE");
  const base::FilePath binary_path(FILE_PATH_LITERAL("/path/to/soda"));
  const base::FilePath en_us_path(FILE_PATH_LITERAL("/path/to/en_us"));
  ConfigureInstalledEnUsSoda(binary_path, en_us_path);

  std::unique_ptr<FakeUtilitySpeechRecognitionService> fake_service;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>
              receiver) {
        fake_service = std::make_unique<FakeUtilitySpeechRecognitionService>(
            std::move(receiver));
      }));

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
  ASSERT_NE(fake_service, nullptr);
  EXPECT_TRUE(IsSpeechRecognitionServiceBound());
  EXPECT_EQ(fake_service->recognizer_count(), 1u);
  EXPECT_EQ(fake_service->last_primary_language_name(), "en-US");
  EXPECT_EQ(profile()->GetPrefs()->GetString(prefs::kLiveCaptionLanguageCode),
            "sv-SE");
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       BindRecognizerWithUnsupportedLiveCaptionPrefFallbackDoesNotCrash) {
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "sv-SE");
  const base::FilePath binary_path(FILE_PATH_LITERAL("/path/to/soda"));
  const base::FilePath en_us_path(FILE_PATH_LITERAL("/path/to/en_us"));
  ConfigureInstalledEnUsSoda(binary_path, en_us_path);

  bool launch_called = false;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>) {
        launch_called = true;
      }));

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
  EXPECT_FALSE(launch_called);
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());

  // Even if the utility service is already running for another session, an
  // unsupported fallback language must not be forwarded to it.
  mojo::PendingRemote<media::mojom::SpeechRecognitionService> service_remote;
  FakeUtilitySpeechRecognitionService fake_service(
      service_remote.InitWithNewPipeAndPassReceiver());
  BindFakeUtilityService(std::move(service_remote));

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer2;
  FakeSpeechRecognitionRecognizerClient client2;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver2(&client2);
  auto options2 = media::mojom::SpeechRecognitionOptions::New();
  options2->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options2->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options2->language = std::nullopt;

  base::test::TestFuture<bool> future2;
  service()->BindRecognizer(recognizer2.BindNewPipeAndPassReceiver(),
                            client_receiver2.BindNewPipeAndPassRemote(),
                            std::move(options2), future2.GetCallback());
  EXPECT_FALSE(future2.Get());
  EXPECT_EQ(fake_service.recognizer_count(), 0u);
  EXPECT_EQ(profile()->GetPrefs()->GetString(prefs::kLiveCaptionLanguageCode),
            "sv-SE");
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       LaunchUsesInstalledLiveCaptionLanguageAsDefaultAndLogsMetric) {
  base::HistogramTester histogram_tester;
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "fr-FR");

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetFilePath(prefs::kSodaBinaryPath,
                           base::FilePath(FILE_PATH_LITERAL("/path/to/soda")));
  local_state->SetFilePath(prefs::kSodaEnUsConfigPath,
                           base::FilePath(FILE_PATH_LITERAL("/path/to/en_us")));
  local_state->SetFilePath(prefs::kSodaFrFrConfigPath,
                           base::FilePath(FILE_PATH_LITERAL("/path/to/fr_fr")));
  base::ListValue registered_languages;
  registered_languages.Append("en-US");
  registered_languages.Append("fr-FR");
  local_state->SetList(prefs::kSodaRegisteredLanguagePacks,
                       std::move(registered_languages));

  std::unique_ptr<FakeUtilitySpeechRecognitionService> fake_service;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>
              receiver) {
        fake_service = std::make_unique<FakeUtilitySpeechRecognitionService>(
            std::move(receiver));
      }));

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
  ASSERT_NE(fake_service, nullptr);
  EXPECT_EQ(fake_service->last_primary_language_name(), "fr-FR");
  EXPECT_EQ(fake_service->last_config_paths().size(), 2u);
  histogram_tester.ExpectUniqueSample("Accessibility.LiveCaption.SodaLanguage",
                                      LanguageCode::kFrFr, 1);
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       LaunchFallsBackToTargetLanguageWhenLiveCaptionPackUninstalled) {
  base::HistogramTester histogram_tester;
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "fr-FR");
  ConfigureInstalledEnUsSoda(
      base::FilePath(FILE_PATH_LITERAL("/path/to/soda")),
      base::FilePath(FILE_PATH_LITERAL("/path/to/en_us")));

  std::unique_ptr<FakeUtilitySpeechRecognitionService> fake_service;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>
              receiver) {
        fake_service = std::make_unique<FakeUtilitySpeechRecognitionService>(
            std::move(receiver));
      }));

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
  ASSERT_NE(fake_service, nullptr);
  EXPECT_EQ(fake_service->last_primary_language_name(), "en-US");
  EXPECT_EQ(fake_service->last_config_paths().size(), 1u);
  EXPECT_FALSE(fake_service->last_config_paths().contains("fr-FR"));
  histogram_tester.ExpectUniqueSample("Accessibility.LiveCaption.SodaLanguage",
                                      LanguageCode::kFrFr, 1);
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       UninstalledRequestLanguageDoesNotLaunch) {
  ConfigureInstalledEnUsSoda(
      base::FilePath(FILE_PATH_LITERAL("/path/to/soda")),
      base::FilePath(FILE_PATH_LITERAL("/path/to/en_us")));

  bool launch_called = false;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>) {
        launch_called = true;
      }));

  // Supported request language ("fr-FR") whose language pack is not installed.
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "en-US");
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> recognizer;
  FakeSpeechRecognitionRecognizerClient client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      client_receiver(&client);
  auto options = media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->language = "fr-FR";

  base::test::TestFuture<bool> future;
  service()->BindRecognizer(recognizer.BindNewPipeAndPassReceiver(),
                            client_receiver.BindNewPipeAndPassRemote(),
                            std::move(options), future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(launch_called);
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
}

TEST_F(ChromeSpeechRecognitionServiceTest,
       BindAudioSourceWithUnsupportedLiveCaptionPrefDoesNotLaunch) {
  ConfigureInstalledEnUsSoda(
      base::FilePath(FILE_PATH_LITERAL("/path/to/soda")),
      base::FilePath(FILE_PATH_LITERAL("/path/to/en_us")));

  bool launch_called = false;
  SetLaunchServiceCallback(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::SpeechRecognitionService>) {
        launch_called = true;
      }));

  // `BindAudioSourceSpeechRecognitionContext` with unsupported Live Caption
  // preference ("sv-SE") enters `LaunchIfNotRunning(std::nullopt)` directly.
  profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode, "sv-SE");
  mojo::Remote<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_context;
  service()->BindAudioSourceSpeechRecognitionContext(
      audio_source_context.BindNewPipeAndPassReceiver());
  audio_source_context.FlushForTesting();
  EXPECT_FALSE(audio_source_context.is_connected());
  EXPECT_FALSE(launch_called);
  EXPECT_FALSE(IsSpeechRecognitionServiceBound());
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

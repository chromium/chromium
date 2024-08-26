// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service_factory.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/fake_speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "content/public/test/browser_test.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace ash {

namespace {
static constexpr int kDefaultSampleRateMs = 16000;
static constexpr int kDefaultPollingTimesHz = 10;
static constexpr char kAlternativeLanguageName[] = "es-ES";
static constexpr char kDefaultLanguageName[] = "en-US";
}  // namespace

// We need to swap out the device audio system for a fake one.
class MockAudioSystem : public media::AudioSystem {
 public:
  MockAudioSystem() = default;
  ~MockAudioSystem() override = default;
  MockAudioSystem(const MockAudioSystem&) = delete;
  MockAudioSystem& operator=(const MockAudioSystem&) = delete;

  // media::AudioSystem overrides:
  MOCK_METHOD2(GetInputStreamParameters,
               void(const std::string& device_id,
                    OnAudioParamsCallback callback));
  MOCK_METHOD2(GetOutputStreamParameters,
               void(const std::string& device_id,
                    OnAudioParamsCallback on_params_cb));
  MOCK_METHOD1(HasInputDevices, void(OnBoolCallback on_has_devices_cb));
  MOCK_METHOD1(HasOutputDevices, void(OnBoolCallback on_has_devices_cb));
  MOCK_METHOD2(GetDeviceDescriptions,
               void(bool for_input,
                    OnDeviceDescriptionsCallback on_descriptions_cp));
  MOCK_METHOD2(GetAssociatedOutputDeviceID,
               void(const std::string& input_device_id,
                    OnDeviceIdCallback on_device_id_cb));
  MOCK_METHOD2(GetInputDeviceInfo,
               void(const std::string& input_device_id,
                    OnInputDeviceInfoCallback on_input_device_info_cb));
};

// Creates and returns a stub audio system that reports a reasonable default for
// audio device parameters.
std::unique_ptr<media::AudioSystem> CreateStubAudioSystem() {
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), kDefaultSampleRateMs,
      kDefaultSampleRateMs / (kDefaultPollingTimesHz * 2));

  std::unique_ptr<MockAudioSystem> stub_audio_system =
      std::make_unique<MockAudioSystem>();
  EXPECT_CALL(*stub_audio_system, GetInputStreamParameters(_, _))
      .WillRepeatedly(
          [params](auto, MockAudioSystem::OnAudioParamsCallback cb) {
            std::move(cb).Run(params);
          });

  return stub_audio_system;
}

// Runs the system live caption service backed by a fake audio system and SODA
// installation.
class SystemLiveCaptionServiceTest
    : public InProcessBrowserTest,
      public speech::FakeSpeechRecognitionService::Observer {
 public:
  SystemLiveCaptionServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOnDeviceSpeechRecognition,
                              features::kSystemLiveCaption,
                              media::kLiveCaptionMultiLanguage},
        /*disabled_features=*/{});
  }

  ~SystemLiveCaptionServiceTest() override = default;
  SystemLiveCaptionServiceTest(const SystemLiveCaptionServiceTest&) = delete;
  SystemLiveCaptionServiceTest& operator=(const SystemLiveCaptionServiceTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreUserProfileMappingForTests);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    primary_profile_ = browser()->profile();

    // Create an additional profile. We will verify that its caption bubble is
    // inactive, since only the primary profile should be processing system
    // audio.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    const base::FilePath profile_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    secondary_profile_ =
        &profiles::testing::CreateProfileSync(profile_manager, profile_path);
    CHECK(secondary_profile_);

    // Replace our CrosSpeechRecognitionService with a fake one.
    fake_speech_recognition_service_ =
        CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
            ->SetTestingSubclassFactoryAndUse(
                primary_profile_, base::BindOnce([](content::BrowserContext*) {
                  return std::make_unique<
                      speech::FakeSpeechRecognitionService>();
                }));
    fake_speech_recognition_service_->AddObserver(this);

    // Pass in an inert audio system backend.
    SystemLiveCaptionServiceFactory::GetInstance()
        ->GetForProfile(primary_profile_)
        ->set_audio_system_factory_for_testing(
            base::BindRepeating(&CreateStubAudioSystem));

    // Don't actually try to download SODA.
    speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();

    // Use English as our caption language.
    primary_profile_->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                            speech::kUsEnglishLocale);
  }

  ::captions::CaptionBubbleController* GetCaptionBubbleController(
      Profile* profile) const {
    return ::captions::LiveCaptionControllerFactory::GetInstance()
        ->GetForProfile(profile)
        ->caption_bubble_controller_for_testing();
  }

  void SetLiveCaptionsPref(bool enabled) {
    primary_profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                             enabled);
    base::RunLoop().RunUntilIdle();
  }

  void SetLanguagePref(const std::string& language) {
    primary_profile_->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                            language);
    base::RunLoop().RunUntilIdle();
  }

  // Emit the given text from our fake speech recognition service.
  void EmulateRecognizedSpeech(const std::string& text) {
    ASSERT_TRUE(current_audio_fetcher_);
    current_audio_fetcher_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(text, /*is_final=*/false));
    base::RunLoop().RunUntilIdle();
  }

  // Meet the preconditions for live captioning so that our logic-under-test
  // starts executing.
  void StartLiveCaptioning() {
    SetLiveCaptionsPref(/*enabled=*/true);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::GetLanguageCode(primary_profile_->GetPrefs()->GetString(
            prefs::kLiveCaptionLanguageCode)));
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
    // Events must propogate, so we wait after install.
    base::RunLoop().RunUntilIdle();
    SystemLiveCaptionServiceFactory::GetInstance()
        ->GetForProfile(primary_profile_)
        ->OnNonChromeOutputStarted();
    base::RunLoop().RunUntilIdle();
  }

  // FakeSpeechRecognitionService::Observer
  void OnRecognizerBound(
      speech::FakeSpeechRecognizer* bound_recognizer) override {
    if (bound_recognizer->recognition_options()->recognizer_client_type ==
        media::mojom::RecognizerClientType::kLiveCaption) {
      current_audio_fetcher_ = bound_recognizer->GetWeakPtr();
    }
  }

  // Unowned.
  raw_ptr<Profile, DanglingUntriaged> primary_profile_;
  raw_ptr<Profile, DanglingUntriaged> secondary_profile_;

  // current_audio_fetcher_ is a speech recognizer fake that is used to assert
  // correct behavior when a session is started by the SystemLiveCaptionService.
  // When a session is started `OnRecognizerBound` is invoked which will
  // populate the current_audio_fetcher_ with the correct audio fetcher.  If
  // this pointer is null then that means that the SystemLiveCapitonService
  // has yet to start a session, so if we want to assert that a session
  // hasn't been started yet thus far in a test we can expect this
  // pointer to be null.
  base::WeakPtr<speech::FakeSpeechRecognizer> current_audio_fetcher_ = nullptr;

  raw_ptr<speech::FakeSpeechRecognitionService, DanglingUntriaged>
      fake_speech_recognition_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that system audio is processed only when all our preconditions are
// satisfied.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, Triggering) {
  // We should be waiting for the feature to be enabled and for SODA to be
  // installed.
  EXPECT_FALSE(current_audio_fetcher_);

  // Enable feature.
  SetLiveCaptionsPref(/*enabled=*/true);

  // We should still be waiting for SODA to be installed.
  EXPECT_FALSE(current_audio_fetcher_);

  // Fake successful language pack install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();

  // We should be waiting for the base binary too.
  EXPECT_FALSE(current_audio_fetcher_);

  // Fake successful base binary install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  base::RunLoop().RunUntilIdle();

  // After language and binary install, still should be false until output is
  // triggered.  The client should be created at this point though.
  ASSERT_TRUE(current_audio_fetcher_);
  EXPECT_FALSE(current_audio_fetcher_->is_capturing_audio());

  // Start audio.
  // Set audio output running.
  SystemLiveCaptionServiceFactory::GetInstance()
      ->GetForProfile(primary_profile_)
      ->OnNonChromeOutputStarted();
  base::RunLoop().RunUntilIdle();

  // Should now be processing system audio.
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // Now turn off live captioning.
  SetLiveCaptionsPref(/*enabled=*/false);

  // This should stop audio fetching.
  EXPECT_FALSE(current_audio_fetcher_);
}

// Test that feature is gated on successful SODA install.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, SodaError) {
  // Enable feature so that we start listening for SODA install status.
  SetLiveCaptionsPref(/*enabled=*/true);

  // Fake successful base binary install but failed language install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();

  // Our language is not yet installed, so we shouldn't be processing audio.
  EXPECT_FALSE(current_audio_fetcher_);
}

// Tests that our feature listens to the correct SODA language.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, SodaIrrelevantError) {
  // Set audio output running
  auto* live_caption_service =
      SystemLiveCaptionServiceFactory::GetInstance()->GetForProfile(
          primary_profile_);
  live_caption_service->OnNonChromeOutputStarted();
  // Enable feature so that we start listening for SODA install status.
  SetLiveCaptionsPref(/*enabled=*/true);

  // Fake successful base binary install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  base::RunLoop().RunUntilIdle();

  // Fake failed install of an unrelated language.
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kFrFr);
  base::RunLoop().RunUntilIdle();

  // Our language is not yet installed, so we shouldn't be processing audio.
  // Therefore the current_audio_fetcher_ should be null.
  EXPECT_FALSE(current_audio_fetcher_);

  // Fake successful install of our language.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();
  // Tell the caption service audio is running again. This is needed since we
  // don't actually go to a fake cras audio system in this test.
  live_caption_service->OnNonChromeOutputStarted();
  base::RunLoop().RunUntilIdle();
  // We should have ignored the unrelated error.
  ASSERT_TRUE(current_audio_fetcher_);
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());
}

// Test that captions are only dispatched for the primary profile.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, DispatchToProfile) {
  StartLiveCaptioning();

  // Capture fake audio.
  EmulateRecognizedSpeech("System audio caption");
  ASSERT_TRUE(current_audio_fetcher_);
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // Transcribed speech should be displayed from the primary profile.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_FALSE(primary_bubble->IsGenericErrorMessageVisibleForTesting());
  EXPECT_EQ("System audio caption",
            primary_bubble->GetBubbleLabelTextForTesting());

  // Transcribed speech should _not_ be shown for any other profiles.
  EXPECT_EQ(nullptr, GetCaptionBubbleController(secondary_profile_));
}

IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, StartStopStart) {
  StartLiveCaptioning();

  // Capture fake audio.
  EmulateRecognizedSpeech("System audio caption");
  ASSERT_TRUE(current_audio_fetcher_);
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // Transcribed speech should be displayed from the primary profile.
  // The added captions are all added as non-finals, so they over-write not
  // append.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_FALSE(primary_bubble->IsGenericErrorMessageVisibleForTesting());
  EXPECT_EQ("System audio caption",
            primary_bubble->GetBubbleLabelTextForTesting());

  // Stop
  SystemLiveCaptionServiceFactory::GetInstance()
      ->GetForProfile(primary_profile_)
      ->OnNonChromeOutputStopped();
  EmulateRecognizedSpeech(" more after stop ");
  EXPECT_EQ(" more after stop ",
            primary_bubble->GetBubbleLabelTextForTesting());
  // Idle stop.
  base::RunLoop().RunUntilIdle();

  // Start again.
  SystemLiveCaptionServiceFactory::GetInstance()
      ->GetForProfile(primary_profile_)
      ->OnNonChromeOutputStarted();
  EmulateRecognizedSpeech(" and yet more ");

  EXPECT_EQ(" and yet more ", primary_bubble->GetBubbleLabelTextForTesting());
  // Transcribed speech should _not_ be shown for any other profiles.
  EXPECT_EQ(nullptr, GetCaptionBubbleController(secondary_profile_));
}

// Test that we can cease transcription by closing the bubble UI.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, EarlyStopping) {
  StartLiveCaptioning();

  // Fake some speech.
  EmulateRecognizedSpeech("System audio caption");

  // Bubble UI should be active to show transcribed speech.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);

  // Emulate closing bubble UI.
  primary_bubble->CloseActiveModelForTesting();

  // Fake detection of more speech, to which the bubble should respond by
  // requesting an early stop.
  EmulateRecognizedSpeech("More system audio captions");

  // The speech recognition service should have received the early stop request.
  // The client will be deleted.
  ASSERT_FALSE(current_audio_fetcher_);
}

// Test that the UI is closed when transcription is complete.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, EndOfStream) {
  StartLiveCaptioning();
  ASSERT_TRUE(current_audio_fetcher_);

  // Fake some speech.
  EmulateRecognizedSpeech("System audio caption");

  // Bubble UI should be active to show transcribed speech.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());

  // Emulate end of audio stream.
  current_audio_fetcher_->MarkDone();
  base::RunLoop().RunUntilIdle();

  // Bubble should not be shown since there is no more audio.
  primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_FALSE(primary_bubble->IsWidgetVisibleForTesting());
}

// Test that an error message is shown if something goes wrong.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, ServiceError) {
  StartLiveCaptioning();
  ASSERT_TRUE(current_audio_fetcher_);

  // Fake some speech.
  EmulateRecognizedSpeech("System audio caption");

  // Bubble UI should be active to show transcribed speech.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_FALSE(primary_bubble->IsGenericErrorMessageVisibleForTesting());

  // Emulate recognition error.
  current_audio_fetcher_->SendSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();

  // Bubble should still be shown and should display error text.
  primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_TRUE(primary_bubble->IsGenericErrorMessageVisibleForTesting());
}

// Tests that the System Live Caption Service uses the correct language as set
// by the kLiveCaptionLanguageCode preference.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, UsesCorrectLanguage) {
  SetLanguagePref(kAlternativeLanguageName);
  StartLiveCaptioning();
  ASSERT_TRUE(current_audio_fetcher_);

  // retrieve the recognition options struct passed to the recognition service.
  // we use this to assert that the correct language was passed to the service.
  const media::mojom::SpeechRecognitionOptions* recognition_options =
      current_audio_fetcher_->recognition_options();

  // Should now be processing system audio.
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // Assert language is correct.
  ASSERT_NE(recognition_options, nullptr);
  EXPECT_EQ(std::string(kAlternativeLanguageName),
            recognition_options->language.value());
}

// When a language changes in the middle of a session the service must switch
// out the speech recognition client for a new one with the selected language.
// This tests that while there are non chrome outputs running that the session
// restarts automatically.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest,
                       SwitchesLanguageCorrectly) {
  StartLiveCaptioning();
  ASSERT_TRUE(current_audio_fetcher_);

  // retrieve the recognition options struct passed to the recognition service.
  // we use this to assert that the correct language was passed to the service.
  const media::mojom::SpeechRecognitionOptions* recognition_options =
      current_audio_fetcher_->recognition_options();

  // Should now be processing system audio.
  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // Assert language is correct.
  ASSERT_NE(recognition_options, nullptr);
  ASSERT_TRUE(recognition_options->language.has_value());
  EXPECT_EQ(std::string(kDefaultLanguageName),
            recognition_options->language.value());

  // This should restart the recognizer with the correct language.  The
  // language pack will be installed by the live caption controller and then
  // the SODA Installer will notify the SystemLiveCaptionService.
  SetLanguagePref(kAlternativeLanguageName);

  // For this test case we want to switch while output is running so that we
  // restart the session without explicitly calling OnNonChromeOutputStarted.
  SystemLiveCaptionServiceFactory::GetInstance()
      ->GetForProfile(primary_profile_)
      ->set_num_non_chrome_output_streams_for_testing(
          /*num_output_streams=*/1);

  // Until SODA installs we should do nothing.  The Client will be created at
  // this point so we can assert that the current audio fetcher is not capturing
  // audio.
  EXPECT_FALSE(current_audio_fetcher_);

  // Emulate successful SODA installation from LiveCaptionController.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kAlternativeLanguageName));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(current_audio_fetcher_->is_capturing_audio());

  // We destroy the old options struct when resetting the speech recogntion
  // client.
  recognition_options = current_audio_fetcher_->recognition_options();

  ASSERT_NE(recognition_options, nullptr);
  ASSERT_TRUE(recognition_options->language.has_value());
  EXPECT_EQ(std::string(kAlternativeLanguageName),
            recognition_options->language.value());
}

}  // namespace ash

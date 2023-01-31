// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service_factory.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
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
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace ash {

namespace {
static constexpr int kDefaultSampleRateMs = 16000;
static constexpr int kDefaultPollingTimesHz = 10;
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
class SystemLiveCaptionServiceTest : public InProcessBrowserTest {
 public:
  SystemLiveCaptionServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOnDeviceSpeechRecognition,
                              features::kSystemLiveCaption},
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
        profiles::testing::CreateProfileSync(profile_manager, profile_path);
    CHECK(secondary_profile_);

    // Replace our CrosSpeechRecognitionService with a fake one. We can pass a
    // unique_ptr into this lambda since it is only called once (despite being
    // "repeating").
    auto service = std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_speech_recognition_service_ = service.get();
    const auto spawn_test_service =
        base::BindRepeating([](std::unique_ptr<KeyedService> s,
                               content::BrowserContext*) { return s; },
                            base::Passed(std::move(service)));
    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(primary_profile_, spawn_test_service);

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

  void SetLiveCaptionsPref(Profile* profile, bool enabled) {
    primary_profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                             enabled);
    base::RunLoop().RunUntilIdle();
  }

  // Emit the given text from our fake speech recognition service.
  void EmulateRecognizedSpeech(const std::string& text) {
    fake_speech_recognition_service_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(text, /*is_final=*/false));
    base::RunLoop().RunUntilIdle();
  }

  // Meet the preconditions for live captioning so that our logic-under-test
  // starts executing.
  void StartLiveCaptioning() {
    SetLiveCaptionsPref(primary_profile_, /*enabled=*/true);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
    base::RunLoop().RunUntilIdle();
  }

  // Unowned.
  Profile* primary_profile_;
  Profile* secondary_profile_;
  speech::FakeSpeechRecognitionService* fake_speech_recognition_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that system audio is processed only when all our preconditions are
// satisfied.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, Triggering) {
  // We should be waiting for the feature to be enabled and for SODA to be
  // installed.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());

  // Enable feature.
  SetLiveCaptionsPref(primary_profile_, /*enabled=*/true);

  // We should still be waiting for SODA to be installed.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());

  // Fake successful language pack install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();

  // We should be waiting for the base binary too.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());

  // Fake successful base binary install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  base::RunLoop().RunUntilIdle();

  // Should now be processing system audio.
  EXPECT_TRUE(fake_speech_recognition_service_->is_capturing_audio());

  // Now turn off live captioning.
  SetLiveCaptionsPref(primary_profile_, /*enabled=*/false);

  // This should stop audio fetching.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());
}

// Test that feature is gated on successful SODA install.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, SodaError) {
  // Enable feature so that we start listening for SODA install status.
  SetLiveCaptionsPref(primary_profile_, /*enabled=*/true);

  // Fake successful base binary install but failed language install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();

  // Our language is not yet installed, so we shouldn't be processing audio.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());
}

// Tests that our feature listens to the correct SODA language.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, SodaIrrelevantError) {
  // Enable feature so that we start listening for SODA install status.
  SetLiveCaptionsPref(primary_profile_, /*enabled=*/true);

  // Fake successful base binary install.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  base::RunLoop().RunUntilIdle();

  // Fake failed install of an unrelated language.
  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kFrFr);
  base::RunLoop().RunUntilIdle();

  // Our language is not yet installed, so we shouldn't be processing audio.
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());

  // Fake successful install of our language.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  base::RunLoop().RunUntilIdle();

  // We should have ignored the unrelated error.
  EXPECT_TRUE(fake_speech_recognition_service_->is_capturing_audio());
}

// Test that captions are only dispatched for the primary profile.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, DispatchToProfile) {
  StartLiveCaptioning();

  // Capture fake audio.
  EmulateRecognizedSpeech("System audio caption");
  EXPECT_TRUE(fake_speech_recognition_service_->is_capturing_audio());

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
  EXPECT_FALSE(fake_speech_recognition_service_->is_capturing_audio());
}

// Test that the UI is closed when transcription is complete.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, EndOfStream) {
  StartLiveCaptioning();

  // Fake some speech.
  EmulateRecognizedSpeech("System audio caption");

  // Bubble UI should be active to show transcribed speech.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());

  // Emulate end of audio stream.
  fake_speech_recognition_service_->MarkDone();
  base::RunLoop().RunUntilIdle();

  // Bubble should not be shown since there is no more audio.
  primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_FALSE(primary_bubble->IsWidgetVisibleForTesting());
}

// Test that an error message is shown if something goes wrong.
IN_PROC_BROWSER_TEST_F(SystemLiveCaptionServiceTest, ServiceError) {
  StartLiveCaptioning();

  // Fake some speech.
  EmulateRecognizedSpeech("System audio caption");

  // Bubble UI should be active to show transcribed speech.
  auto* primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_FALSE(primary_bubble->IsGenericErrorMessageVisibleForTesting());

  // Emulate recognition error.
  fake_speech_recognition_service_->SendSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();

  // Bubble should still be shown and should display error text.
  primary_bubble = GetCaptionBubbleController(primary_profile_);
  ASSERT_NE(nullptr, primary_bubble);
  EXPECT_TRUE(primary_bubble->IsWidgetVisibleForTesting());
  EXPECT_TRUE(primary_bubble->IsGenericErrorMessageVisibleForTesting());
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"

#include <map>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/fake_speech_recognizer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "content/public/test/browser_test.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoDefault;
using ::testing::InvokeWithoutArgs;

namespace {
static constexpr int kDefaultSampleRate = 16000;
static constexpr int kDefaultPollingTimesPerSecond = 10;
}  // namespace

class MockSpeechRecognizerDelegate : public SpeechRecognizerDelegate {
 public:
  MockSpeechRecognizerDelegate() {}

  base::WeakPtr<MockSpeechRecognizerDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD3(
      OnSpeechResult,
      void(const std::u16string& text,
           bool is_final,
           const std::optional<media::SpeechRecognitionResult>& timing));
  MOCK_METHOD1(OnSpeechSoundLevelChanged, void(int16_t));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(SpeechRecognizerStatus));
  MOCK_METHOD0(OnSpeechRecognitionStopped, void());
  MOCK_METHOD1(OnLanguageIdentificationEvent,
               void(media::mojom::LanguageIdentificationEventPtr));

 private:
  base::WeakPtrFactory<MockSpeechRecognizerDelegate> weak_factory_{this};
};

class MockAudioSystem : public media::AudioSystem {
 public:
  MockAudioSystem() = default;
  ~MockAudioSystem() override = default;
  MockAudioSystem(const MockAudioSystem&) = delete;
  MockAudioSystem& operator=(const MockAudioSystem&) = delete;

  // media::AudioSystem:
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback callback) override {
    std::move(callback).Run(params_[device_id]);
  }
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

  void SetInputStreamParameters(
      const std::string& device_id,
      const std::optional<media::AudioParameters>& params) {
    params_[device_id] = params;
  }

 private:
  std::map<std::string, std::optional<media::AudioParameters>> params_;
};

// Tests SpeechRecognitionRecognizerClientImpl plumbing with a fake
// SpeechRecognitionService. Does not do end-to-end audio fetching or test SODA
// on device.
class SpeechRecognitionRecognizerClientImplTest
    : public InProcessBrowserTest,
      public speech::FakeSpeechRecognitionService::Observer {
 public:
  SpeechRecognitionRecognizerClientImplTest() = default;
  ~SpeechRecognitionRecognizerClientImplTest() override = default;
  SpeechRecognitionRecognizerClientImplTest(
      const SpeechRecognitionRecognizerClientImplTest&) = delete;
  SpeechRecognitionRecognizerClientImplTest& operator=(
      const SpeechRecognitionRecognizerClientImplTest&) = delete;

  // FakeSpeechRecognitionService::Observer
  void OnRecognizerBound(
      speech::FakeSpeechRecognizer* bound_recognizer) override {
    if (bound_recognizer->recognition_options()->recognizer_client_type ==
        media::mojom::RecognizerClientType::kDictation) {
      fake_speech_recognizer_ = bound_recognizer->GetWeakPtr();
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Replaces normal CrosSpeechRecognitionService with a fake one.
    CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&SpeechRecognitionRecognizerClientImplTest::
                                    CreateTestSpeechRecognitionService,
                                base::Unretained(this)));
    mock_speech_delegate_ =
        std::make_unique<testing::StrictMock<MockSpeechRecognizerDelegate>>();
    // Fake that SODA is installed.
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> CreateTestSpeechRecognitionService(
      content::BrowserContext* context) {
    std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
        std::make_unique<speech::FakeSpeechRecognitionService>();
    fake_service_ = fake_service.get();
    fake_service_->AddObserver(this);
    return std::move(fake_service);
  }

  void ConstructRecognizerAndWaitForReady() {
    base::RunLoop loop;
    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
        .WillOnce(InvokeWithoutArgs(&loop, &base::RunLoop::Quit))
        .RetiresOnSaturation();
    recognizer_ = std::make_unique<SpeechRecognitionRecognizerClientImpl>(
        mock_speech_delegate_->GetWeakPtr(), browser()->profile(),
        media::AudioDeviceDescription::kDefaultDeviceId,
        media::mojom::SpeechRecognitionOptions::New(
            media::mojom::SpeechRecognitionMode::kIme,
            /*enable_formatting=*/false,
            /*language=*/"en-US",
            /*is_server_based=*/false,
            media::mojom::RecognizerClientType::kDictation));
    loop.Run();
  }

  void StartAndWaitForRecognizing() {
    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_RECOGNIZING))
        .Times(1)
        .RetiresOnSaturation();
    recognizer_->Start();
    fake_speech_recognizer_->WaitForRecognitionStarted();
    base::RunLoop().RunUntilIdle();
  }

  void StartListeningWithAudioParams(
      const std::optional<media::AudioParameters>& params) {
    std::unique_ptr<MockAudioSystem> mock_audio_system =
        std::make_unique<MockAudioSystem>();
    mock_audio_system->SetInputStreamParameters(
        media::AudioDeviceDescription::kDefaultDeviceId, params);
    recognizer_->set_audio_system_for_testing(std::move(mock_audio_system));
    StartAndWaitForRecognizing();
  }

  std::unique_ptr<testing::StrictMock<MockSpeechRecognizerDelegate>>
      mock_speech_delegate_;
  std::unique_ptr<SpeechRecognitionRecognizerClientImpl> recognizer_;

  // Unowned.
  raw_ptr<speech::FakeSpeechRecognitionService, DanglingUntriaged>
      fake_service_;
  base::WeakPtr<speech::FakeSpeechRecognizer> fake_speech_recognizer_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       SetsUpServiceConnection) {
  ConstructRecognizerAndWaitForReady();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       StartsCapturingAudio) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();
  EXPECT_FALSE(fake_speech_recognizer_->is_capturing_audio());

  // Toggle a few times.
  for (int i = 0; i < 2; i++) {
    StartAndWaitForRecognizing();
    EXPECT_TRUE(fake_speech_recognizer_->is_capturing_audio());

    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_STOPPING))
        .Times(1)
        .RetiresOnSaturation();

    EXPECT_CALL(*mock_speech_delegate_,
                OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_READY))
        .Times(1)
        .RetiresOnSaturation();

    EXPECT_CALL(*mock_speech_delegate_, OnSpeechRecognitionStopped())
        .Times(1)
        .RetiresOnSaturation();

    recognizer_->Stop();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(fake_speech_recognizer_->is_capturing_audio());
  }
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       ReceivesRecognitionEvents) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();

  StartAndWaitForRecognizing();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_IN_SPEECH))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(std::u16string(u"All mammals have hair"), false,
                             testing::_))
      .Times(1)
      .RetiresOnSaturation();
  fake_speech_recognizer_->SendSpeechRecognitionResult(
      media::SpeechRecognitionResult("All mammals have hair", false));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechResult(
                  std::u16string(u"All mammals drink milk from their mothers"),
                  true, testing::_))
      .Times(1)
      .RetiresOnSaturation();
  fake_speech_recognizer_->SendSpeechRecognitionResult(
      media::SpeechRecognitionResult(
          "All mammals drink milk from their mothers", true));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       ReceivesErrors) {
  testing::InSequence seq;
  ConstructRecognizerAndWaitForReady();

  EXPECT_CALL(*mock_speech_delegate_,
              OnSpeechRecognitionStateChanged(SPEECH_RECOGNIZER_ERROR))
      .Times(1)
      .RetiresOnSaturation();
  fake_speech_recognizer_->SendSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       UsesReturnedParametersIfFramesPerBufferIsSlowEnough) {
  fake_service_->set_multichannel_supported(true);
  int sample_rate = 10000;
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), sample_rate,
      sample_rate / (kDefaultPollingTimesPerSecond / 2));
  ConstructRecognizerAndWaitForReady();
  StartListeningWithAudioParams(params);

  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            fake_speech_recognizer_->device_id());
  ASSERT_TRUE(fake_speech_recognizer_->audio_parameters());
  EXPECT_TRUE(fake_speech_recognizer_->audio_parameters()->Equals(params));
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       SlowerPollingIfFramesPerBufferIsTooShort) {
  fake_service_->set_multichannel_supported(true);
  int sample_rate = 20000;
  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), sample_rate,
      sample_rate / (kDefaultPollingTimesPerSecond * 2));
  ConstructRecognizerAndWaitForReady();
  StartListeningWithAudioParams(params);

  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            fake_speech_recognizer_->device_id());
  ASSERT_TRUE(fake_speech_recognizer_->audio_parameters());
  EXPECT_EQ(media::CHANNEL_LAYOUT_STEREO,
            fake_speech_recognizer_->audio_parameters()->channel_layout());
  // Picks a larger frames_per_buffer such that sample_rate/frames_per_buffer =
  // kDefaultPollingTimesPerSecond.
  EXPECT_EQ(sample_rate / kDefaultPollingTimesPerSecond,
            fake_speech_recognizer_->audio_parameters()->frames_per_buffer());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       SetsToSingleChannelIfMultichannelNotSupported) {
  fake_service_->set_multichannel_supported(false);
  int sample_rate = 20000;
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Stereo(),
                                sample_rate,
                                sample_rate / kDefaultPollingTimesPerSecond);
  ConstructRecognizerAndWaitForReady();
  StartListeningWithAudioParams(params);

  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            fake_speech_recognizer_->device_id());
  ASSERT_TRUE(fake_speech_recognizer_->audio_parameters());
  EXPECT_EQ(sample_rate,
            fake_speech_recognizer_->audio_parameters()->sample_rate());
  EXPECT_EQ(media::CHANNEL_LAYOUT_MONO,
            fake_speech_recognizer_->audio_parameters()->channel_layout());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionRecognizerClientImplTest,
                       DefaultParameters) {
  fake_service_->set_multichannel_supported(true);
  ConstructRecognizerAndWaitForReady();
  StartListeningWithAudioParams(std::nullopt);

  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            fake_speech_recognizer_->device_id());
  ASSERT_TRUE(fake_speech_recognizer_->audio_parameters());
  EXPECT_EQ(kDefaultSampleRate,
            fake_speech_recognizer_->audio_parameters()->sample_rate());
  EXPECT_EQ(
      kDefaultPollingTimesPerSecond,
      fake_speech_recognizer_->audio_parameters()->sample_rate() /
          fake_speech_recognizer_->audio_parameters()->frames_per_buffer());
  EXPECT_EQ(media::CHANNEL_LAYOUT_STEREO,
            fake_speech_recognizer_->audio_parameters()->channel_layout());
}

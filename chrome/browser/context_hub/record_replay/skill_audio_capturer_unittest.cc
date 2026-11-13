// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_audio_capturer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/fake_speech_recognizer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "components/soda/mock_soda_installer.h"
#else
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#endif

namespace context_hub {
namespace {

using base::test::RunUntil;

ProfileKeyedServiceFactory* GetSpeechRecognitionServiceFactoryForTest() {
#if BUILDFLAG(IS_CHROMEOS)
  return CrosSpeechRecognitionServiceFactory::GetInstanceForTest();
#else
  return SpeechRecognitionServiceFactory::GetInstanceForTest();
#endif
}

class FakeAudioSystem : public media::AudioSystem {
 public:
  FakeAudioSystem() = default;
  FakeAudioSystem(const FakeAudioSystem&) = delete;
  FakeAudioSystem& operator=(const FakeAudioSystem&) = delete;
  ~FakeAudioSystem() override = default;

  // media::AudioSystem:
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback on_params_cb) override {
    last_device_id_ = device_id;
    pending_params_cb_ = std::move(on_params_cb);
  }
  void GetOutputStreamParameters(const std::string& device_id,
                                 OnAudioParamsCallback on_params_cb) override {}
  void HasInputDevices(OnBoolCallback on_has_devices_cb) override {}
  void HasOutputDevices(OnBoolCallback on_has_devices_cb) override {}
  void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cb) override {}
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_cb) override {}
  void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_cb) override {}

  const std::string& last_device_id() const { return last_device_id_; }
  bool has_pending_params_callback() const {
    return !pending_params_cb_.is_null();
  }
  void ReplyWithInputStreamParameters(
      const std::optional<media::AudioParameters>& params) {
    std::move(pending_params_cb_).Run(params);
  }

 private:
  std::string last_device_id_;
  OnAudioParamsCallback pending_params_cb_;
};

class SkillAudioCapturerTest
    : public testing::Test,
      public speech::FakeSpeechRecognitionService::Observer {
 public:
  SkillAudioCapturerTest()
      : fake_speech_service_(
            CHECK_DEREF(GetSpeechRecognitionServiceFactoryForTest()
                            ->SetTestingSubclassFactoryAndUse<
                                speech::FakeSpeechRecognitionService>(
                                &profile_,
                                base::BindOnce([](content::BrowserContext*) {
                                  return std::make_unique<
                                      speech::FakeSpeechRecognitionService>();
                                })))) {
    fake_speech_service_->AddObserver(this);
  }
  SkillAudioCapturerTest(const SkillAudioCapturerTest&) = delete;
  SkillAudioCapturerTest& operator=(const SkillAudioCapturerTest&) = delete;
  ~SkillAudioCapturerTest() override {
    fake_speech_service_->RemoveObsever(this);
  }

  // speech::FakeSpeechRecognitionService::Observer:
  void OnRecognizerBound(
      speech::FakeSpeechRecognizer* bound_recognizer) override {
    bind_recognizer_count_++;
    bound_recognizer_ = bound_recognizer->GetWeakPtr();
  }

  void OnTranscriptUpdate(const std::string& text,
                          base::TimeDelta timestamp,
                          bool is_final) {
    last_text_ = text;
    last_timestamp_ = timestamp;
    last_is_final_ = is_final;
    update_count_++;
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kOnDeviceSpeechRecognition};
  speech::MockSodaInstaller soda_installer_;
#endif
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  int bind_recognizer_count_ = 0;
  base::WeakPtr<speech::FakeSpeechRecognizer> bound_recognizer_;
  std::string last_text_;
  base::TimeDelta last_timestamp_;
  bool last_is_final_ = false;
  int update_count_ = 0;
  const raw_ref<speech::FakeSpeechRecognitionService> fake_speech_service_;
  SkillAudioCapturer capturer_{
      &profile_,
      base::BindRepeating(&SkillAudioCapturerTest::OnTranscriptUpdate,
                          base::Unretained(this))};
};

TEST_F(SkillAudioCapturerTest, StartAndStopLifecycle) {
  EXPECT_FALSE(capturer_.is_recording());
  capturer_.Stop();
  EXPECT_FALSE(capturer_.is_recording());

  capturer_.Start();
  EXPECT_TRUE(capturer_.is_recording());
  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 1; }));
  ASSERT_TRUE(bound_recognizer_);

  // Calling `Start()` again while already recording should be a no-op.
  capturer_.Start();
  EXPECT_EQ(bind_recognizer_count_, 1);

  capturer_.Stop();
  EXPECT_FALSE(capturer_.is_recording());
  EXPECT_TRUE(RunUntil([&] { return !bound_recognizer_; }));
}

TEST_F(SkillAudioCapturerTest, ForwardsTranscripts) {
  capturer_.Start();
  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 1; }));
  ASSERT_TRUE(bound_recognizer_);
  bound_recognizer_->Start(mojo::NullRemote(), /*device_id=*/"default",
                           media::AudioParameters());

  media::SpeechRecognitionResult media_timed_result("open tab",
                                                    /*is_final=*/false);
  media::TimingInformation& media_timing =
      media_timed_result.timing_information.emplace();
  media_timing.audio_start_time = base::Milliseconds(50);
  media_timing.audio_end_time = base::Milliseconds(100);
  media_timing.originating_media_timestamps =
      std::vector<media::MediaTimestampRange>{
          {.start = base::Milliseconds(120), .end = base::Milliseconds(170)}};
  bound_recognizer_->SendSpeechRecognitionResult(media_timed_result);
  ASSERT_TRUE(RunUntil([&] { return update_count_ == 1; }));
  EXPECT_EQ(last_text_, "open tab");
  EXPECT_EQ(last_timestamp_, base::Milliseconds(120));
  EXPECT_FALSE(last_is_final_);

  media::SpeechRecognitionResult partial_result("click", /*is_final=*/false);
  media::TimingInformation& partial_timing =
      partial_result.timing_information.emplace();
  partial_timing.audio_start_time = base::Milliseconds(150);
  partial_timing.audio_end_time = base::Milliseconds(300);
  bound_recognizer_->SendSpeechRecognitionResult(partial_result);
  ASSERT_TRUE(RunUntil([&] { return update_count_ == 2; }));
  EXPECT_EQ(last_text_, "click");
  EXPECT_EQ(last_timestamp_, base::Milliseconds(150));
  EXPECT_FALSE(last_is_final_);

  // Untimed transcript falls back to elapsed time since `Start()`; empty
  // transcript is ignored.
  media::SpeechRecognitionResult final_result("click submit",
                                              /*is_final=*/true);
  bound_recognizer_->SendSpeechRecognitionResult(final_result);
  ASSERT_TRUE(RunUntil([&] { return update_count_ == 3; }));
  EXPECT_EQ(last_text_, "click submit");
  EXPECT_GE(last_timestamp_, base::TimeDelta());
  EXPECT_LE(last_timestamp_, base::Seconds(10));
  EXPECT_TRUE(last_is_final_);

  media::SpeechRecognitionResult empty_result("", /*is_final=*/false);
  bound_recognizer_->SendSpeechRecognitionResult(empty_result);
  EXPECT_EQ(update_count_, 3);

  capturer_.Stop();
}

TEST_F(SkillAudioCapturerTest, CaptureForwardsConvertedAudio) {
  capturer_.Start();
  const base::TimeTicks after_start = base::TimeTicks::Now();
  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 1; }));
  ASSERT_TRUE(bound_recognizer_);
  EXPECT_FALSE(bound_recognizer_->is_capturing_audio());

  capturer_.OnAudioCapturedForTesting(/*audio_bus=*/nullptr, after_start);

  std::unique_ptr<media::AudioBus> bus =
      media::AudioBus::Create(/*channels=*/1, /*frames=*/4);
  bus->channel(0)[0] = 0.0f;
  bus->channel(0)[1] = 0.5f;
  bus->channel(0)[2] = -0.5f;
  bus->channel(0)[3] = 1.0f;

  capturer_.OnAudioCapturedForTesting(std::move(bus),
                                      after_start + base::Milliseconds(100));

  // `FakeSpeechRecognizer::SendSpeechRecognitionResult()` verifies that
  // `SendAudioToSpeechRecognitionService()` was called (`has_received_audio_`),
  // and `SkillAudioCapturer`'s reply sets `is_capturing_audio()` to true.
  bound_recognizer_->SendSpeechRecognitionResult(
      media::SpeechRecognitionResult("ok", /*is_final=*/true));
  EXPECT_TRUE(
      RunUntil([&] { return bound_recognizer_->is_capturing_audio(); }));
  EXPECT_EQ(update_count_, 1);

  capturer_.Stop();
}

TEST_F(SkillAudioCapturerTest, StopsOnErrors) {
  capturer_.Start();
  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 1; }));
  ASSERT_TRUE(bound_recognizer_);
  bound_recognizer_->MarkDone();
  EXPECT_TRUE(RunUntil([&] { return !capturer_.is_recording(); }));

  capturer_.Start();
  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 2; }));
  ASSERT_TRUE(bound_recognizer_);
  bound_recognizer_->SendSpeechRecognitionError();
  EXPECT_TRUE(RunUntil([&] { return !capturer_.is_recording(); }));
}

TEST_F(SkillAudioCapturerTest, UsesNativeDeviceAudioParameters) {
  auto fake_audio_system = std::make_unique<FakeAudioSystem>();
  FakeAudioSystem* raw_audio_system = fake_audio_system.get();
  capturer_.SetAudioSystemForTesting(std::move(fake_audio_system));

  capturer_.Start();
  ASSERT_TRUE(raw_audio_system->has_pending_params_callback());
  EXPECT_EQ(raw_audio_system->last_device_id(),
            media::AudioDeviceDescription::kDefaultDeviceId);

  const media::AudioParameters mac_device_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(),
      /*sample_rate=*/48000,
      /*frames_per_buffer=*/512);
  raw_audio_system->ReplyWithInputStreamParameters(mac_device_params);

  EXPECT_TRUE(capturer_.is_recording());
  EXPECT_EQ(capturer_.audio_params_for_testing().sample_rate(), 48000);
  EXPECT_EQ(capturer_.audio_params_for_testing().channel_layout(),
            media::CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(capturer_.audio_params_for_testing().frames_per_buffer(), 512);

  ASSERT_TRUE(RunUntil([&] { return bind_recognizer_count_ == 1; }));
  ASSERT_TRUE(bound_recognizer_);

  std::unique_ptr<media::AudioBus> stereo_bus =
      media::AudioBus::Create(/*channels=*/2, /*frames=*/4);
  stereo_bus->Zero();
  stereo_bus->channel(0)[0] = 0.5f;
  stereo_bus->channel(1)[0] = 0.5f;
  capturer_.OnAudioCapturedForTesting(std::move(stereo_bus),
                                      base::TimeTicks::Now());

  bound_recognizer_->SendSpeechRecognitionResult(
      media::SpeechRecognitionResult("mac audio", /*is_final=*/true));
  EXPECT_TRUE(
      RunUntil([&] { return bound_recognizer_->is_capturing_audio(); }));
  EXPECT_EQ(last_text_, "mac audio");

  capturer_.Stop();
  EXPECT_EQ(capturer_.audio_params_for_testing().sample_rate(), 16000);
}

TEST_F(SkillAudioCapturerTest,
       StopsWhenNoInputDeviceOrIgnoresCancelledParamsCallback) {
  auto fake_audio_system = std::make_unique<FakeAudioSystem>();
  FakeAudioSystem* raw_audio_system = fake_audio_system.get();
  capturer_.SetAudioSystemForTesting(std::move(fake_audio_system));

  // Missing input device (`std::nullopt`) stops recording cleanly.
  capturer_.Start();
  ASSERT_TRUE(raw_audio_system->has_pending_params_callback());
  raw_audio_system->ReplyWithInputStreamParameters(std::nullopt);
  EXPECT_FALSE(capturer_.is_recording());

  // Stopping while `GetInputStreamParameters()` is in flight invalidates the
  // pending callback.
  capturer_.Start();
  ASSERT_TRUE(raw_audio_system->has_pending_params_callback());
  capturer_.Stop();
  EXPECT_FALSE(capturer_.is_recording());

  const media::AudioParameters valid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(),
      /*sample_rate=*/44100,
      /*frames_per_buffer=*/4410);
  raw_audio_system->ReplyWithInputStreamParameters(valid_params);
  EXPECT_FALSE(capturer_.is_recording());
  EXPECT_EQ(capturer_.audio_params_for_testing().sample_rate(), 16000);
}

}  // namespace
}  // namespace context_hub

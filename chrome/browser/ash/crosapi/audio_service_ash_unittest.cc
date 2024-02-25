// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/audio_service_ash.h"

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

using ::testing::_;
using ::testing::IsTrue;
using ::testing::StrictMock;

class MockCallbacks {
 public:
  MOCK_METHOD2(GetMuteResponse, void(bool, bool));
  MOCK_METHOD1(GetDevicesResponse,
               void(std::optional<std::vector<mojom::AudioDeviceInfoPtr>>));
  MOCK_METHOD1(SetMuteResponse, void(bool));
  MOCK_METHOD1(SetActiveDeviceListsResponse, void(bool));
  MOCK_METHOD1(SetPropertiesResponse, void(bool));
};

class MockAudioChangeObserver : public mojom::AudioChangeObserver {
 public:
  // mojom::AudioChangeObserver
  void OnDeviceListChanged(
      std::vector<mojom::AudioDeviceInfoPtr> devices) override {
    OnDeviceListChangedMock(devices);
  }

  void OnLevelChanged(const std::string& id, int32_t level) override {
    OnLevelChangedMock(id, level);
  }

  void OnMuteChanged(bool is_input, bool is_muted) override {
    OnMuteChangedMock(is_input, is_muted);
  }

  // mock methods to check a correct call
  MOCK_METHOD(void,
              OnDeviceListChangedMock,
              (const std::vector<mojom::AudioDeviceInfoPtr>& devices));
  MOCK_METHOD(void, OnLevelChangedMock, (const std::string& id, int32_t level));
  MOCK_METHOD(void, OnMuteChangedMock, (bool is_input, bool is_muted));

  void Wait() { EXPECT_TRUE(waiter.Wait()); }

  auto GetRemote() { return receiver_.BindNewPipeAndPassRemote(); }

  void Quit() { waiter.SetValue(); }

 private:
  mojo::Receiver<crosapi::mojom::AudioChangeObserver> receiver_{this};
  base::test::TestFuture<void> waiter;
};

class AudioServiceAshTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing_profile_ = TestingProfile::Builder().Build();
    audio_service_ash_.Initialize(testing_profile_.get());
    audio_service_ash_.AddAudioChangeObserver(mock_observer_.GetRemote());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ash::ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  StrictMock<MockCallbacks> mock_callbacks_;
  StrictMock<MockAudioChangeObserver> mock_observer_;
  std::unique_ptr<TestingProfile> testing_profile_;
  AudioServiceAsh audio_service_ash_;
};

TEST_F(AudioServiceAshTest, GetDevices) {
  auto empty_filter = crosapi::mojom::DeviceFilter::New();
  mojom::AudioService::GetDevicesCallback cb = base::BindOnce(
      &MockCallbacks::GetDevicesResponse, base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, GetDevicesResponse(IsTrue()));

  audio_service_ash_.GetDevices(std::move(empty_filter), std::move(cb));
}

TEST_F(AudioServiceAshTest, GetMute) {
  mojom::AudioService::GetMuteCallback cb = base::BindOnce(
      &MockCallbacks::GetMuteResponse, base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, GetMuteResponse(true, false));

  audio_service_ash_.GetMute(crosapi::mojom::StreamType::kOutput,
                             std::move(cb));
}

TEST_F(AudioServiceAshTest, SetActiveDeviceListsNull) {
  mojom::AudioService::SetActiveDeviceListsCallback cb =
      base::BindOnce(&MockCallbacks::SetActiveDeviceListsResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetActiveDeviceListsResponse(true));

  audio_service_ash_.SetActiveDeviceLists(nullptr, std::move(cb));
}

TEST_F(AudioServiceAshTest, SetActiveDeviceListsBadId) {
  auto dev_lists = crosapi::mojom::DeviceIdLists::New();
  const std::string kBadId = "-1";
  dev_lists->outputs.push_back(kBadId);
  mojom::AudioService::SetActiveDeviceListsCallback cb =
      base::BindOnce(&MockCallbacks::SetActiveDeviceListsResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetActiveDeviceListsResponse(false));

  audio_service_ash_.SetActiveDeviceLists(std::move(dev_lists), std::move(cb));
}

TEST_F(AudioServiceAshTest, SetActiveDeviceListsSpeaker) {
  auto dev_lists = crosapi::mojom::DeviceIdLists::New();
  const std::string kSpeakerId =
      base::NumberToString(0x100000001);  // from FakeCrasAudioClient
  dev_lists->outputs.push_back(kSpeakerId);
  mojom::AudioService::SetActiveDeviceListsCallback cb =
      base::BindOnce(&MockCallbacks::SetActiveDeviceListsResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetActiveDeviceListsResponse(true));
  EXPECT_CALL(mock_observer_, OnLevelChangedMock(kSpeakerId, _)).WillOnce([&] {
    mock_observer_.Quit();
  });

  audio_service_ash_.SetActiveDeviceLists(std::move(dev_lists), std::move(cb));
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, SetPropertiesBadId) {
  const std::string kBadId = "-1";
  auto props = crosapi::mojom::AudioDeviceProperties::New(50);
  mojom::AudioService::SetPropertiesCallback cb =
      base::BindOnce(&MockCallbacks::SetPropertiesResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetPropertiesResponse(false));

  audio_service_ash_.SetProperties(kBadId, std::move(props), std::move(cb));
}

TEST_F(AudioServiceAshTest, SetPropertiesNullProp) {
  const std::string kId = base::NumberToString(0x100000001);
  mojom::AudioService::SetPropertiesCallback cb =
      base::BindOnce(&MockCallbacks::SetPropertiesResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetPropertiesResponse(false));

  audio_service_ash_.SetProperties(kId, nullptr, std::move(cb));
}

TEST_F(AudioServiceAshTest, SetPropertiesHeadphone) {
  // taken from FakeCrasAudioClient, device must be active to trigger event
  const std::string kId = base::NumberToString(0x200000001);
  const int kNewVolume = 12;
  auto props = crosapi::mojom::AudioDeviceProperties::New(kNewVolume);
  mojom::AudioService::SetPropertiesCallback cb =
      base::BindOnce(&MockCallbacks::SetPropertiesResponse,
                     base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetPropertiesResponse(true));
  EXPECT_CALL(mock_observer_, OnLevelChangedMock(kId, kNewVolume))
      .WillOnce([&] { mock_observer_.Quit(); });

  audio_service_ash_.SetProperties(kId, std::move(props), std::move(cb));
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, SetMuteOut) {
  const bool kNewMuteValue = true;
  mojom::AudioService::SetMuteCallback cb = base::BindOnce(
      &MockCallbacks::SetMuteResponse, base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetMuteResponse(true));
  EXPECT_CALL(mock_observer_, OnMuteChangedMock(false, kNewMuteValue))
      .WillOnce([&] { mock_observer_.Quit(); });

  audio_service_ash_.SetMute(crosapi::mojom::StreamType::kOutput, kNewMuteValue,
                             std::move(cb));
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, SetMuteIn) {
  const bool kNewMuteValue = true;
  mojom::AudioService::SetMuteCallback cb = base::BindOnce(
      &MockCallbacks::SetMuteResponse, base::Unretained(&mock_callbacks_));

  EXPECT_CALL(mock_callbacks_, SetMuteResponse(true));
  EXPECT_CALL(mock_observer_, OnMuteChangedMock(true, kNewMuteValue))
      .WillOnce([&] { mock_observer_.Quit(); });

  audio_service_ash_.SetMute(crosapi::mojom::StreamType::kInput, kNewMuteValue,
                             std::move(cb));
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnMuteChangedOut) {
  const bool kNewMuteValue = true;

  EXPECT_CALL(mock_observer_, OnMuteChangedMock(false, kNewMuteValue))
      .WillOnce([&] { mock_observer_.Quit(); });

  cras_audio_handler_.Get().SetOutputMute(kNewMuteValue);
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnMuteChangedIn) {
  const bool kNewMuteValue = true;

  EXPECT_CALL(mock_observer_, OnMuteChangedMock(true, kNewMuteValue))
      .WillOnce([&] { mock_observer_.Quit(); });

  cras_audio_handler_.Get().SetInputMute(
      kNewMuteValue, ash::CrasAudioHandler::InputMuteChangeMethod::kOther);
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnLevelChangedOut) {
  const uint64_t kId = 0x200000001;  // taken from FakeCrasAudioClient
  const int kNewVolume = 36;

  EXPECT_CALL(mock_observer_,
              OnLevelChangedMock(base::NumberToString(kId), kNewVolume))
      .WillOnce([&] { mock_observer_.Quit(); });

  ash::FakeCrasAudioClient::Get()->NotifyOutputNodeVolumeChangedForTesting(
      kId, kNewVolume);
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnLevelChangedIn) {
  const uint64_t kId = 0x200000002;  // taken from FakeCrasAudioClient
  const int kNewGain = 36;

  EXPECT_CALL(mock_observer_,
              OnLevelChangedMock(base::NumberToString(kId), kNewGain))
      .WillOnce([&] { mock_observer_.Quit(); });

  ash::FakeCrasAudioClient::Get()->NotifyInputNodeGainChangedForTesting(
      kId, kNewGain);
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnDeviceListChangedAdd) {
  ash::AudioNode new_device;
  new_device.id = 5;

  EXPECT_CALL(mock_observer_, OnDeviceListChangedMock(_)).WillOnce([&] {
    mock_observer_.Quit();
  });

  ash::FakeCrasAudioClient::Get()->InsertAudioNodeToList(new_device);
  mock_observer_.Wait();
}

TEST_F(AudioServiceAshTest, OnDeviceListChangedRemove) {
  const uint64_t id = cras_audio_handler_.Get().GetPrimaryActiveInputNode();
  ash::FakeCrasAudioClient::Get()->RemoveAudioNodeFromList(id);

  // Active input device changes and level is set to preference when devices are
  // changed.
  const int kPreferredGain = 50;
  uint64_t input_id = cras_audio_handler_.Get().GetPrimaryActiveInputNode();
  EXPECT_EQ(kPreferredGain, cras_audio_handler_.Get().GetInputGainPercent());
  EXPECT_CALL(mock_observer_, OnLevelChangedMock(base::NumberToString(input_id),
                                                 kPreferredGain));

  // Device list change happens after input level change.
  EXPECT_CALL(mock_observer_, OnDeviceListChangedMock(_)).WillOnce([&] {
    mock_observer_.Quit();
  });

  mock_observer_.Wait();
}

}  // namespace crosapi

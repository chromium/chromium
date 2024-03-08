// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

MATCHER(VideoCaptureDeviceInfoEq, "") {
  return std::get<0>(arg).descriptor.device_id ==
         std::get<1>(arg).descriptor.device_id;
}

MATCHER(MediaStreamDeviceInfoEq, "") {
  return std::get<0>(arg).id == std::get<1>(arg).id &&
         std::get<0>(arg).type == std::get<1>(arg).type;
}

template <typename T>
std::vector<T>::const_iterator GetIterForInfo(const std::vector<T>& infos,
                                              const T& info) {
  for (auto iter = infos.begin(); iter < infos.end(); ++iter) {
    if (media_prefs::internal::DeviceInfoToUniqueId(*iter) ==
        media_prefs::internal::DeviceInfoToUniqueId(info)) {
      return iter;
    }
  }

  return infos.end();
}

}  // namespace

class CaptureDeviceRankingTest : public testing::Test {
  void SetUp() override { media_prefs::RegisterUserPrefs(prefs_.registry()); }

 protected:
  void UpdateAudioRanking(
      const media::AudioDeviceDescription& preferred_device,
      const std::vector<media::AudioDeviceDescription>& infos) {
    media_prefs::UpdateAudioDevicePreferenceRanking(
        prefs_, GetIterForInfo(infos, preferred_device), infos);
  }

  void UpdateVideoRanking(
      const media::VideoCaptureDeviceInfo& preferred_device,
      const std::vector<media::VideoCaptureDeviceInfo>& infos) {
    media_prefs::UpdateVideoDevicePreferenceRanking(
        prefs_, GetIterForInfo(infos, preferred_device), infos);
  }

  void ExpectAudioRanking(
      std::vector<media::AudioDeviceDescription> infos,
      const std::vector<media::AudioDeviceDescription>& expected_output) {
    media_prefs::PreferenceRankAudioDeviceInfos(prefs_, infos);
    EXPECT_THAT(infos, expected_output);
  }

  void ExpectVideoRanking(
      std::vector<media::VideoCaptureDeviceInfo> infos,
      const std::vector<media::VideoCaptureDeviceInfo>& expected_output) {
    media_prefs::PreferenceRankVideoDeviceInfos(prefs_, infos);
    EXPECT_THAT(
        infos, testing::Pointwise(VideoCaptureDeviceInfoEq(), expected_output));
  }

  TestingPrefServiceSimple prefs_;
};

TEST_F(CaptureDeviceRankingTest, PreferenceRankVideoDeviceInfos) {
  const media::VideoCaptureDeviceInfo kIntegratedCamera(
      {/*display_name=*/"Integrated Camera",
       /*device_id=*/"integrated_camera"});
  const media::VideoCaptureDeviceInfo kUsbCamera({/*display_name=*/"USB Camera",
                                                  /*device_id=*/"usb_camera"});
  const media::VideoCaptureDeviceInfo kDevice3({/*display_name=*/"Device 3",
                                                /*device_id=*/"device_3"});
  const media::VideoCaptureDeviceInfo kDeviceLost(
      {/*display_name=*/"Device Lost",
       /*device_id=*/"device_lost"});
  const media::VideoCaptureDeviceInfo kDeviceExtra(
      {/*display_name=*/"Device Extra",
       /*device_id=*/"device_extra"});

  UpdateVideoRanking(
      /*preferred_device=*/kIntegratedCamera,
      /*infos=*/{kDeviceLost, kDevice3, kIntegratedCamera, kUsbCamera});

  UpdateVideoRanking(
      /*preferred_device=*/kUsbCamera,
      /*infos=*/{kDevice3, kIntegratedCamera, kUsbCamera});

  ExpectVideoRanking(
      /*infos=*/{kDevice3, kIntegratedCamera, kUsbCamera},
      /*expected_output=*/{kUsbCamera, kIntegratedCamera, kDevice3});

  // Remove USB camera and select the integrated camera. Integrated
  // should stay below USB in the ranking  because the USB camera isn't
  // present.
  UpdateVideoRanking(/*preferred_device=*/kIntegratedCamera,
                     /*infos=*/{kDevice3, kIntegratedCamera});

  ExpectVideoRanking(
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedCamera},
      /*expected_output=*/{kIntegratedCamera, kDevice3, kDeviceExtra});

  // Bring back USB camera. The USB camera should be ranked as the most
  // preferred device even without an update.
  ExpectVideoRanking(
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedCamera, kUsbCamera},
      /*expected_output=*/{kUsbCamera, kIntegratedCamera, kDevice3,
                           kDeviceExtra});
}

TEST_F(CaptureDeviceRankingTest, PreferenceRankAudioDeviceInfos) {
  const media::AudioDeviceDescription kIntegratedMic(
      {/*device_name=*/"Integrated Mic",
       /*unique_id=*/"integrated_mic",
       /*group_id=*/"integrated_group"});
  const media::AudioDeviceDescription kUsbMic({
      /*device_name=*/"USB Mic",
      /*unique_id=*/"usb_mic",
      /*group_id=*/"usb_group",
  });
  const media::AudioDeviceDescription kDevice3({/*device_name=*/"Device 3",
                                                /*unique_id=*/"device_3",
                                                /*group_id=*/"device_group"});
  const media::AudioDeviceDescription kDeviceLost(
      {/*device_name=*/"Device Lost",
       /*unique_id=*/"device_lost",
       /*group_id=*/"device_group"});
  const media::AudioDeviceDescription kDeviceExtra(
      {/*device_name=*/"Device Extra",
       /*unique_id=*/"device_extra",
       /*group_id=*/"device_group"});

  UpdateAudioRanking(
      /*preferred_device=*/kIntegratedMic,
      /*infos=*/{kDeviceLost, kDevice3, kIntegratedMic, kUsbMic});

  UpdateAudioRanking(/*preferred_device=*/kUsbMic,
                     /*infos=*/{kDevice3, kIntegratedMic, kUsbMic});

  ExpectAudioRanking(
      /*infos=*/{kDevice3, kIntegratedMic, kUsbMic},
      /*expected_output=*/{kUsbMic, kIntegratedMic, kDevice3});

  // Remove USB mic and select the integrated mic. Integrated
  // should stay below USB in the ranking  because the USB mic isn't
  // present.
  UpdateAudioRanking(/*preferred_device=*/kIntegratedMic,
                     /*infos=*/{kDevice3, kIntegratedMic});

  ExpectAudioRanking(
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedMic},
      /*expected_output=*/{kIntegratedMic, kDevice3, kDeviceExtra});

  // Bring back USB mic. The USB mic should be ranked as the most
  // preferred device even without an update.
  ExpectAudioRanking(
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedMic, kUsbMic},
      /*expected_output=*/{kUsbMic, kIntegratedMic, kDevice3, kDeviceExtra});
}

TEST_F(CaptureDeviceRankingTest, InitializeAudioDeviceInfosRanking) {
  const media::AudioDeviceDescription kIntegratedMic(
      {/*device_name=*/"Integrated Mic",
       /*unique_id=*/"integrated_mic",
       /*group_id=*/"integrated_group"});
  const media::AudioDeviceDescription kUsbMic({
      /*device_name=*/"USB Mic",
      /*unique_id=*/"usb_mic",
      /*group_id=*/"usb_group",
  });

  prefs_.registry()->RegisterStringPref(
      prefs::kDefaultAudioCaptureDeviceDeprecated, "");

  // The default device is set to "" so the output should be unmodified.
  ExpectAudioRanking({kIntegratedMic, kUsbMic}, {kIntegratedMic, kUsbMic});

  // The default device isn't in the passed device list, so the output should be
  // unmodified.
  prefs_.SetString(prefs::kDefaultAudioCaptureDeviceDeprecated, "not_found_id");
  ExpectAudioRanking({kIntegratedMic, kUsbMic}, {kIntegratedMic, kUsbMic});

  // The default device is USB mic so the device ranking gets initialized to put
  // that first.
  prefs_.SetString(prefs::kDefaultAudioCaptureDeviceDeprecated,
                   kUsbMic.unique_id);
  ExpectAudioRanking({kIntegratedMic, kUsbMic}, {kUsbMic, kIntegratedMic});

  // The device ranking pref now has a value, so use that instead of the default
  // device pref.
  UpdateAudioRanking(kIntegratedMic, {kIntegratedMic, kUsbMic});
  ExpectAudioRanking({kUsbMic, kIntegratedMic}, {kIntegratedMic, kUsbMic});
}

TEST_F(CaptureDeviceRankingTest, InitializeVideoDeviceInfosRanking) {
  const media::VideoCaptureDeviceInfo kIntegratedCamera(
      {/*display_name=*/"Integrated Camera",
       /*device_id=*/"integrated_camera"});
  const media::VideoCaptureDeviceInfo kUsbCamera({/*display_name=*/"USB Camera",
                                                  /*device_id=*/"usb_camera"});

  prefs_.registry()->RegisterStringPref(
      prefs::kDefaultVideoCaptureDeviceDeprecated, "");

  // The default device is set to "" so the output should be unmodified.
  ExpectVideoRanking({kIntegratedCamera, kUsbCamera},
                     {kIntegratedCamera, kUsbCamera});

  // The default device isn't in the passed device list, so the output should be
  // unmodified.
  prefs_.SetString(prefs::kDefaultVideoCaptureDeviceDeprecated, "not_found_id");
  ExpectVideoRanking({kIntegratedCamera, kUsbCamera},
                     {kIntegratedCamera, kUsbCamera});

  // The default device is usb camera so the device ranking gets Initialized to
  // put that first.
  prefs_.SetString(prefs::kDefaultVideoCaptureDeviceDeprecated,
                   kUsbCamera.descriptor.device_id);
  ExpectVideoRanking({kIntegratedCamera, kUsbCamera},
                     {kUsbCamera, kIntegratedCamera});

  // The device ranking pref now has a value, so use that instead of the default
  // device pref.
  UpdateVideoRanking(kIntegratedCamera, {kIntegratedCamera, kUsbCamera});
  ExpectVideoRanking({kUsbCamera, kIntegratedCamera},
                     {kIntegratedCamera, kUsbCamera});
}

class CaptureDeviceRankingWebMediaDeviceTest
    : public CaptureDeviceRankingTest,
      public testing::WithParamInterface<blink::mojom::MediaDeviceType> {
 protected:
  void UpdateDeviceRanking(
      blink::mojom::MediaDeviceType device_type,
      const blink::WebMediaDeviceInfo& preferred_device,
      const std::vector<blink::WebMediaDeviceInfo>& infos) {
    if (device_type == blink::mojom::MediaDeviceType::kMediaAudioInput) {
      media_prefs::UpdateAudioDevicePreferenceRanking(
          prefs_, GetIterForInfo(infos, preferred_device), infos);
    } else if (device_type == blink::mojom::MediaDeviceType::kMediaVideoInput) {
      media_prefs::UpdateVideoDevicePreferenceRanking(
          prefs_, GetIterForInfo(infos, preferred_device), infos);
    } else {
      FAIL() << "Called with an unsupported type: " << device_type;
    }
  }

  void ExpectDeviceRanking(
      blink::mojom::MediaDeviceType device_type,
      std::vector<blink::WebMediaDeviceInfo> infos,
      const std::vector<blink::WebMediaDeviceInfo>& expected_output) {
    if (device_type == blink::mojom::MediaDeviceType::kMediaAudioInput) {
      media_prefs::PreferenceRankAudioDeviceInfos(prefs_, infos);
      EXPECT_EQ(infos, expected_output);
    } else if (device_type == blink::mojom::MediaDeviceType::kMediaVideoInput) {
      media_prefs::PreferenceRankVideoDeviceInfos(prefs_, infos);
      EXPECT_EQ(infos, expected_output);
    } else {
      FAIL() << "Called with an unsupported type: " << device_type;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    WebMediaDeviceAudioVideoInput,
    CaptureDeviceRankingWebMediaDeviceTest,
    ::testing::Values(blink::mojom::MediaDeviceType::kMediaAudioInput,
                      blink::mojom::MediaDeviceType::kMediaVideoInput));

TEST_P(CaptureDeviceRankingWebMediaDeviceTest, PreferenceWebMediaDeviceInfos) {
  const blink::WebMediaDeviceInfo kIntegratedDevice(
      /*device_id=*/"integrated_device",
      /*label=*/"Integrated Device",
      /*group_id=*/"integrated_group");
  const blink::WebMediaDeviceInfo kUsbDevice(
      /*device_id=*/"usb_device",
      /*label=*/"USB Device",
      /*group_id=*/"usb_group");
  const blink::WebMediaDeviceInfo kDevice3(
      /*device_id=*/"device_3",
      /*label=*/"Device 3",
      /*group_id=*/"device_group");
  const blink::WebMediaDeviceInfo kDeviceLost(
      /*device_id=*/"device_lost",
      /*label=*/"Device Lost",
      /*group_id=*/"device_group");
  const blink::WebMediaDeviceInfo kDeviceExtra(
      /*device_id=*/"device_extra",
      /*label=*/"Device Extra",
      /*group_id=*/"device_group");

  const blink::mojom::MediaDeviceType device_type = GetParam();

  UpdateDeviceRanking(
      device_type,
      /*preferred_device=*/kIntegratedDevice,
      /*infos=*/{kDeviceLost, kDevice3, kIntegratedDevice, kUsbDevice});

  UpdateDeviceRanking(device_type,
                      /*preferred_device=*/kUsbDevice,
                      /*infos=*/{kDevice3, kIntegratedDevice, kUsbDevice});

  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDevice3, kIntegratedDevice, kUsbDevice},
      /*expected_output=*/{kUsbDevice, kIntegratedDevice, kDevice3});

  // Remove USB device and select the integrated device. Integrated
  // should stay below USB in the ranking  because the USB device isn't
  // present.
  UpdateDeviceRanking(device_type,
                      /*preferred_device=*/kIntegratedDevice,
                      /*infos=*/{kDevice3, kIntegratedDevice});

  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedDevice},
      /*expected_output=*/{kIntegratedDevice, kDevice3, kDeviceExtra});

  // Bring back USB device. The USB device should be ranked as the most
  // preferred device even without an update.
  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedDevice, kUsbDevice},
      /*expected_output=*/
      {kUsbDevice, kIntegratedDevice, kDevice3, kDeviceExtra});
}

class CaptureDeviceRankingMediaStreamDeviceTest
    : public CaptureDeviceRankingTest,
      public testing::WithParamInterface<blink::mojom::MediaStreamType> {
 protected:
  void UpdateDeviceRanking(blink::mojom::MediaStreamType device_type,
                           const blink::MediaStreamDevice& preferred_device,
                           const std::vector<blink::MediaStreamDevice>& infos) {
    if (device_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      media_prefs::UpdateAudioDevicePreferenceRanking(
          prefs_, GetIterForInfo(infos, preferred_device), infos);
    } else if (device_type ==
               blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      media_prefs::UpdateVideoDevicePreferenceRanking(
          prefs_, GetIterForInfo(infos, preferred_device), infos);
    } else {
      FAIL() << "Called with an unsupported type: " << device_type;
    }
  }

  void ExpectDeviceRanking(
      blink::mojom::MediaStreamType device_type,
      std::vector<blink::MediaStreamDevice> infos,
      const std::vector<blink::MediaStreamDevice>& expected_output) {
    if (device_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      media_prefs::PreferenceRankAudioDeviceInfos(prefs_, infos);
      EXPECT_THAT(infos, testing::Pointwise(MediaStreamDeviceInfoEq(),
                                            expected_output));
    } else if (device_type ==
               blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      media_prefs::PreferenceRankVideoDeviceInfos(prefs_, infos);
      EXPECT_THAT(infos, testing::Pointwise(MediaStreamDeviceInfoEq(),
                                            expected_output));
    } else {
      FAIL() << "Called with an unsupported type: " << device_type;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    MediaStreamDeviceAudioVideoInput,
    CaptureDeviceRankingMediaStreamDeviceTest,
    ::testing::Values(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

TEST_P(CaptureDeviceRankingMediaStreamDeviceTest,
       PreferenceMediaStreamDevices) {
  const blink::mojom::MediaStreamType device_type = GetParam();

  const blink::MediaStreamDevice kIntegratedDevice(
      /*type=*/device_type,
      /*id=*/"integrated_device",
      /*name=*/"Integrated Device");
  const blink::MediaStreamDevice kUsbDevice(
      /*type=*/device_type,
      /*id=*/"usb_device",
      /*name=*/"USB Device");
  const blink::MediaStreamDevice kDevice3(
      /*type=*/device_type,
      /*id=*/"device_3",
      /*name=*/"Device 3");
  const blink::MediaStreamDevice kDeviceLost(
      /*type=*/device_type,
      /*id=*/"device_lost",
      /*name=*/"Device Lost");
  const blink::MediaStreamDevice kDeviceExtra(
      /*type=*/device_type,
      /*id=*/"device_extra",
      /*name=*/"Device Extra");

  UpdateDeviceRanking(
      device_type,
      /*preferred_device=*/kIntegratedDevice,
      /*infos=*/{kDeviceLost, kDevice3, kIntegratedDevice, kUsbDevice});

  UpdateDeviceRanking(device_type,
                      /*preferred_device=*/kUsbDevice,
                      /*infos=*/{kDevice3, kIntegratedDevice, kUsbDevice});

  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDevice3, kIntegratedDevice, kUsbDevice},
      /*expected_output=*/{kUsbDevice, kIntegratedDevice, kDevice3});

  // Remove USB device and select the integrated device. Integrated
  // should stay below USB in the ranking  because the USB device isn't
  // present.
  UpdateDeviceRanking(device_type,
                      /*preferred_device=*/kIntegratedDevice,
                      /*infos=*/{kDevice3, kIntegratedDevice});

  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedDevice},
      /*expected_output=*/{kIntegratedDevice, kDevice3, kDeviceExtra});

  // Bring back USB device. The USB device should be ranked as the most
  // preferred device even without an update.
  ExpectDeviceRanking(
      device_type,
      /*infos=*/{kDeviceExtra, kDevice3, kIntegratedDevice, kUsbDevice},
      /*expected_output=*/
      {kUsbDevice, kIntegratedDevice, kDevice3, kDeviceExtra});
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include <array>

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "media/audio/audio_device_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

#if BUILDFLAG(IS_MAC)
TEST(CastMirroringServiceHostTest, DesktopAudioUsesCastLoopbackDeviceOnMac) {
  EXPECT_EQ(CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId(),
            media::AudioDeviceDescription::kLoopbackWithMuteDeviceIdCast);
}
#elif BUILDFLAG(IS_WIN)
TEST(CastMirroringServiceHostTest,
     DesktopAudioDoesNotMuteLocalPlaybackOnWinByDefault) {
  EXPECT_EQ(CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId(),
            media::AudioDeviceDescription::kLoopbackInputDeviceId);
}

TEST(CastMirroringServiceHostTest,
     DesktopAudioDoesNotMuteLocalPlaybackOnWinWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      media_router::kCastDesktopMirroringKeepLocalAudio);
  EXPECT_EQ(CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId(),
            media::AudioDeviceDescription::kLoopbackInputDeviceId);
}

TEST(CastMirroringServiceHostTest,
     DesktopAudioMutesLocalPlaybackOnWinWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      media_router::kCastDesktopMirroringKeepLocalAudio);
  EXPECT_EQ(CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId(),
            media::AudioDeviceDescription::kLoopbackWithMuteDeviceId);
}
#else
TEST(CastMirroringServiceHostTest, DesktopAudioMutesLocalPlayback) {
  EXPECT_EQ(CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId(),
            media::AudioDeviceDescription::kLoopbackWithMuteDeviceId);
}
#endif

// The selected device ID must always be one that the audio service treats as
// a loopback device; otherwise it would be opened as a regular input device.
TEST(CastMirroringServiceHostTest, DesktopAudioDeviceIsLoopbackDevice) {
  EXPECT_TRUE(media::AudioDeviceDescription::IsLoopbackDevice(
      CastMirroringServiceHost::GetDesktopAudioLoopbackDeviceId()));
}

TEST(CastMirroringServiceHostTest, TestGetClampedResolution) {
  // {screen width, screen height, capture width, capture height}
  constexpr const auto test_cases = std::to_array<std::array<int, 4>>({
      {1280, 800, 1280, 720},
      {1366, 768, 1280, 720},
      {768, 1366, 1280, 720},
      {1920, 1080, 1920, 1080},
      {1546, 2048, 1920, 1080},
      {2399, 1598, 1920, 1080},
      {2560, 1080, 1920, 1080},
      {2560, 1600, 1920, 1080},
      {3840, 2160, 1920, 1080},
  });

  for (int i = 0; i < 9; i++) {
    DVLOG(1) << "Testing resolution " << test_cases[i][0] << " x "
             << test_cases[i][1];
    EXPECT_EQ(CastMirroringServiceHost::GetClampedResolution(
                  gfx::Size(test_cases[i][0], test_cases[i][1])),
              gfx::Size(test_cases[i][2], test_cases[i][3]));
  }
}

}  // namespace mirroring

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/video_encode_accelerator_mojom_traits.h"

#include <limits>

#include "ash/components/arc/mojom/video_common.mojom.h"
#include "media/base/bitrate.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(BitrateDataViewUnionTraitsTest, ConstantBitrateRoundTrip) {
  media::Bitrate input_bitrate = media::Bitrate::ConstantBitrate(1234u);

  media::Bitrate output_bitrate;
  mojo::test::SerializeAndDeserialize<arc::mojom::Bitrate>(input_bitrate,
                                                           output_bitrate);

  EXPECT_EQ(input_bitrate, output_bitrate);
}

TEST(BitrateDataViewUnionTraitsTest, VariableBitrateRoundTrip) {
  media::Bitrate input_bitrate = media::Bitrate::VariableBitrate(1000u, 2000u);

  media::Bitrate output_bitrate;
  mojo::test::SerializeAndDeserialize<arc::mojom::Bitrate>(input_bitrate,
                                                           output_bitrate);

  EXPECT_EQ(input_bitrate, output_bitrate);
}

TEST(BitrateDataViewUnionTraitsTest, ConstantBitrateMaximumTarget) {
  media::Bitrate input_bitrate =
      media::Bitrate::ConstantBitrate(std::numeric_limits<uint32_t>::max());

  media::Bitrate output_bitrate;
  mojo::test::SerializeAndDeserialize<arc::mojom::Bitrate>(input_bitrate,
                                                           output_bitrate);

  EXPECT_EQ(input_bitrate, output_bitrate);
  EXPECT_EQ(output_bitrate.target_bps(), std::numeric_limits<uint32_t>::max());
}

TEST(BitrateDataViewUnionTraitsTest, VariableBitrateMaximumTargetAndPeak) {
  media::Bitrate input_bitrate =
      media::Bitrate::VariableBitrate(std::numeric_limits<uint32_t>::max(),
                                      std::numeric_limits<uint32_t>::max());

  media::Bitrate output_bitrate;
  mojo::test::SerializeAndDeserialize<arc::mojom::Bitrate>(input_bitrate,
                                                           output_bitrate);

  EXPECT_EQ(input_bitrate, output_bitrate);
  EXPECT_EQ(output_bitrate.target_bps(), std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(output_bitrate.peak_bps(), std::numeric_limits<uint32_t>::max());
}

TEST(VideoEncodeAcceleratorConfigStructTraitTest, RoundTrip) {
  // Spatial layers are needed for construction of the
  // VideoEncodeAccelerator::Config but not used in the final check because they
  // are not transported
  std::vector<::media::VideoEncodeAccelerator::Config::SpatialLayer>
      input_spatial_layers;
  constexpr gfx::Size kBaseSize(320, 180);
  constexpr uint32_t kBaseBitrateBps = 123456u;
  constexpr uint32_t kBaseFramerate = 24u;
  const ::media::Bitrate kBitrate =
      ::media::Bitrate::ConstantBitrate(kBaseBitrateBps);

  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBitrate, kBaseFramerate,
      ::media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera);
  input_config.spatial_layers = input_spatial_layers;
  input_config.inter_layer_pred = ::media::SVCInterLayerPredMode::kOnKeyPic;

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              arc::mojom::VideoEncodeAcceleratorConfig>(input_config,
                                                        output_config));
  // Arc does not transport the fields:
  //  |is_constrained_h264|
  //  |gop_length|
  //  |content_type|
  //  |spatial_layers|
  //  |inter_layer_pred|
  //  |require_low_delay|
  // so we check fields individually rather than checking overall equality of
  // the configs
  EXPECT_EQ(input_config.input_format, output_config.input_format);
  EXPECT_EQ(input_config.input_visible_size, output_config.input_visible_size);
  EXPECT_EQ(input_config.output_profile, output_config.output_profile);
  EXPECT_EQ(input_config.framerate, output_config.framerate);
  EXPECT_EQ(input_config.h264_output_level, output_config.h264_output_level);
  EXPECT_EQ(input_config.storage_type, output_config.storage_type);
  EXPECT_EQ(input_config.bitrate, output_config.bitrate);
}

TEST(VideoEncodeAcceleratorConfigStructTraitTest, RoundTripVariableBitrate) {
  constexpr gfx::Size kBaseSize(320, 180);
  constexpr uint32_t kBaseBitrateBps = 123456u;
  constexpr uint32_t kMaximumBitrate = 999999u;
  const ::media::Bitrate kBitrate =
      ::media::Bitrate::VariableBitrate(kBaseBitrateBps, kMaximumBitrate);
  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBitrate, media::VideoEncodeAccelerator::kDefaultFramerate,
      ::media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera);

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              arc::mojom::VideoEncodeAcceleratorConfig>(input_config,
                                                        output_config));

  EXPECT_EQ(input_config.bitrate, output_config.bitrate);
}

}  // namespace mojo

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kModelId = "AABBCC";
// Corresponds to version = 0 and flags = 0.
constexpr uint8_t kHeaderLeAudioSharing2025 = 0b00000000;
// Corresponds to length of 3 and type of 7 (model ID).
constexpr uint8_t kModelIdExtraFieldHeader = 0b00110111;

// Only used when feature flag is disabled.
const std::string kLongModelId = "1122334455667788";
const std::string kPaddedModelId = "00001111";
const std::string kTrimmedModelId = "001111";
constexpr uint8_t kLongModelIdHeader = 0b00010000;
constexpr uint8_t kPaddedLongModelIdHeader = 0b00001000;

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_decoder {

class FastPairDecoderTest : public testing::Test {
 protected:
  bool HasModelIdString(std::string model_id) {
    std::vector<uint8_t> bytes;
    base::HexStringToBytes(model_id, &bytes);
    return HasModelId(&bytes);
  }
};

// #######################################################################
// Begin: Tests with kFastPairAdvertisingFormat2025 disabled.
// TODO(399163998): Deprecate these tests once the feature is rolled out.
// #######################################################################

TEST_F(FastPairDecoderTest, HasModelId_ThreeByteFormat) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  EXPECT_TRUE(HasModelIdString(kModelId));
}

TEST_F(FastPairDecoderTest, HasModelId_TooShort) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  EXPECT_FALSE(HasModelIdString("11"));
}

TEST_F(FastPairDecoderTest, HasModelId_LongFormat) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00001000)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_TRUE(HasModelId(&bytes));

  bytes = FastPairServiceDataCreator::Builder()
              .SetHeader(0b00001010)
              .SetModelId("1122334455")
              .Build()
              ->CreateServiceData();

  EXPECT_TRUE(HasModelId(&bytes));
}

TEST_F(FastPairDecoderTest, HasModelId_LongInvalidVersion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00101000)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_FALSE(HasModelId(&bytes));
}

TEST_F(FastPairDecoderTest, HasModelId_LongInvalidLength) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00001010)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_FALSE(HasModelId(&bytes));

  bytes = FastPairServiceDataCreator::Builder()
              .SetHeader(0b00000010)
              .SetModelId("11223344")
              .Build()
              ->CreateServiceData();

  EXPECT_FALSE(HasModelId(&bytes));
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_Handles2018Format) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(0b00000110)
                                          .SetModelId(kModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kModelId);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_LongModelId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});

  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kLongModelIdHeader)
                                          .SetModelId(kLongModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kLongModelId);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_LongModelIdTrimmed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairAdvertisingFormat2025});

  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kPaddedLongModelIdHeader)
                                          .SetModelId(kPaddedModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kTrimmedModelId);
}

// #######################################################################
// Begin: Tests with kFastPairAdvertisingFormat2025 enabled, or tests to
// be run for both feature enablement states.
// #######################################################################

TEST_F(FastPairDecoderTest, GetVersion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00110001)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  // Correctly uses 0bVVVVFFFF (2022) format to get version and flags.
  EXPECT_EQ(GetVersion2022(&bytes), GetVersion(&bytes));
  EXPECT_EQ(3, GetVersion(&bytes));
  EXPECT_EQ(1, GetFlags(&bytes));
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForNullData) {
  EXPECT_EQ(GetHexModelIdFromServiceData(nullptr), std::nullopt);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForEmptyData) {
  std::vector<uint8_t> empty;
  EXPECT_EQ(GetHexModelIdFromServiceData(&empty), std::nullopt);
}

TEST_F(FastPairDecoderTest,
       GetHexModelIdFromServiceData_NoResultForUnsupportedFormat) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(0b01100110)
                                          .SetModelId(kModelId)
                                          .Build()
                                          ->CreateServiceData();

  // Since the header does not match 2018 or 2015 Le Audio Sharing format, and
  // the advertisement data is more than 3 bytes, we will not parse the model
  // ID.
  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), std::nullopt);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_ThreeByteData) {
  std::vector<uint8_t> bytes;
  base::HexStringToBytes(kModelId, &bytes);
  EXPECT_EQ(GetHexModelIdFromServiceData(&bytes), kModelId);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_Ignores2018Format) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(0b00000110)
                                          .SetModelId(kModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), std::nullopt);
}

TEST_F(FastPairDecoderTest, GetExtraFields_GetsAllFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});

  // Extra field headers correctly use 0bLLLLTTTT format to describe
  // the length and type of the following fields.
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kHeaderLeAudioSharing2025)
                                          .AddExtraFieldHeader(0b00010001)
                                          .AddExtraField("AA")
                                          .AddExtraFieldHeader(0b00100010)
                                          .AddExtraField("BBBB")
                                          .AddExtraFieldHeader(0b00110011)
                                          .AddExtraField("CCCCCC")
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetExtraField(&service_data, 1), "AA");
  EXPECT_EQ(GetExtraField(&service_data, 2), "BBBB");
  EXPECT_EQ(GetExtraField(&service_data, 3), "CCCCCC");
}

TEST_F(FastPairDecoderTest, GetExtraFields_HandlesIncorrectFieldLengths) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kHeaderLeAudioSharing2025)
                                          .AddExtraFieldHeader(0b00000000)
                                          .AddExtraField(kModelId)
                                          .Build()
                                          ->CreateServiceData();

  // Handle illegal extra field length (0).
  EXPECT_EQ(GetExtraField(&service_data, 0), std::nullopt);

  service_data = FastPairServiceDataCreator::Builder()
                     .SetHeader(kHeaderLeAudioSharing2025)
                     .AddExtraFieldHeader(0b00100000)
                     .AddExtraField("A")
                     .Build()
                     ->CreateServiceData();

  // Handle incorrect extra field length (2), when length is 1.
  EXPECT_EQ(GetExtraField(&service_data, 0), std::nullopt);
}

TEST_F(FastPairDecoderTest,
       GetHexModelIdFromServiceData_LeAudioSharing2025_HandlesStandardFormat) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kHeaderLeAudioSharing2025)
          .AddExtraFieldHeader(kModelIdExtraFieldHeader)
          .AddExtraField(kModelId)
          .Build()
          ->CreateServiceData();

  // Standard model is 3 bytes long, type 7.
  EXPECT_EQ(GetExtraField(&service_data, 7), kModelId);
  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kModelId);
}

TEST_F(FastPairDecoderTest,
       GetHexModelIdFromServiceData_LeAudioSharing2025_HandlesOtherFormats) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kHeaderLeAudioSharing2025)
                                          .AddExtraFieldHeader(0b00010001)
                                          .AddExtraField("DC")
                                          .AddExtraFieldHeader(0b01000111)
                                          .AddExtraField("AABBCCDD")
                                          .AddExtraFieldHeader(0b00010011)
                                          .AddExtraField("DC")
                                          .Build()
                                          ->CreateServiceData();

  // In this example, we sandwiched model ID among other fields and made it 4
  // bytes long.
  EXPECT_EQ(GetExtraField(&service_data, 7), "AABBCCDD");
  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), "AABBCCDD");
}

TEST_F(FastPairDecoderTest,
       GetHexModelIdFromServiceData_LeAudioSharing2025_ModelIdNotFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{ash::features::kFastPairAdvertisingFormat2025},
      /*disabled_features=*/{});
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kHeaderLeAudioSharing2025)
                                          .AddExtraFieldHeader(0b00010001)
                                          .AddExtraField("DC")
                                          .AddExtraFieldHeader(0b00010011)
                                          .AddExtraField("DC")
                                          .Build()
                                          ->CreateServiceData();

  // We did not include an extra field with model ID here, so it won't be found.
  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), std::nullopt);
}

}  // namespace fast_pair_decoder
}  // namespace quick_pair
}  // namespace ash

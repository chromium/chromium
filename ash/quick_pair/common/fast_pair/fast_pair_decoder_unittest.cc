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
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kModelId = "AABBCC";
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

// #######################################################################
// Begin: Tests with kFastPairAdvertisingFormat2025 enabled, or tests to
// be run for both feature enablement states.
// #######################################################################

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForNullData) {
  EXPECT_EQ(GetHexModelIdFromServiceData(nullptr), std::nullopt);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForEmptyData) {
  std::vector<uint8_t> empty;
  EXPECT_EQ(GetHexModelIdFromServiceData(&empty), std::nullopt);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_ThreeByteData) {
  std::vector<uint8_t> bytes;
  base::HexStringToBytes(kModelId, &bytes);
  EXPECT_EQ(GetHexModelIdFromServiceData(&bytes), kModelId);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_LongModelId) {
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kLongModelIdHeader)
                                          .SetModelId(kLongModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kLongModelId);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_LongModelIdTrimmed) {
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kPaddedLongModelIdHeader)
                                          .SetModelId(kPaddedModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(GetHexModelIdFromServiceData(&service_data), kTrimmedModelId);
}

}  // namespace fast_pair_decoder
}  // namespace quick_pair
}  // namespace ash

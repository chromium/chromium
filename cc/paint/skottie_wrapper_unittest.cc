// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "cc/test/lottie_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using ::testing::Eq;
using ::testing::Ne;

TEST(SkottieWrapperTest, LoadsValidLottieFileNonSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, LoadsValidLottieFileSerializable) {
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  EXPECT_TRUE(skottie->is_valid());
}

TEST(SkottieWrapperTest, DetectsInvalidLottieFile) {
  static constexpr base::StringPiece kInvalidJson = "this is invalid json";
  scoped_refptr<SkottieWrapper> skottie =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kInvalidJson.data()),
          kInvalidJson.length()));
  EXPECT_FALSE(skottie->is_valid());
}

TEST(SkottieWrapperTest, IdMatchesForSameLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()) +
              kLottieDataWithoutAssets1.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Eq(skottie_2->id()));
}

TEST(SkottieWrapperTest, IdDoesNotMatchForDifferentLottieFile) {
  scoped_refptr<SkottieWrapper> skottie_1 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets1.data()),
          kLottieDataWithoutAssets1.length()));
  scoped_refptr<SkottieWrapper> skottie_2 =
      SkottieWrapper::CreateNonSerializable(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kLottieDataWithoutAssets2.data()),
          kLottieDataWithoutAssets2.length()));
  ASSERT_TRUE(skottie_1->is_valid());
  ASSERT_TRUE(skottie_2->is_valid());
  EXPECT_THAT(skottie_1->id(), Ne(skottie_2->id()));
}

}  // namespace
}  // namespace cc

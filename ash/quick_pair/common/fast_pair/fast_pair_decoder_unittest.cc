// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_decoder.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const std::string kModelId = "AABBCC";
const std::string kLongModelId = "1122334455667788";
const std::string kPaddedModelId = "00001111";
const std::string kTrimmedModelId = "001111";
constexpr uint8_t kLongModelIdHeader = 0b00010000;
constexpr uint8_t kPaddedLongModelIdHeader = 0b00001000;

// Convenience class with Builder to create byte arrays which represent Fast
// Pair Service Data.
class FastPairServiceDataCreator {
 public:
  class Builder {
   public:
    Builder& SetHeader(uint8_t byte) {
      header_ = byte;
      return *this;
    }

    Builder& SetModelId(std::string model_id) {
      model_id_ = model_id;
      return *this;
    }

    Builder& AddExtraFieldHeader(uint8_t header) {
      extra_field_headers_.push_back(header);
      return *this;
    }

    Builder& AddExtraField(std::string field) {
      extra_fields_.push_back(field);
      return *this;
    }

    std::unique_ptr<FastPairServiceDataCreator> Build() {
      return std::make_unique<FastPairServiceDataCreator>(
          header_, model_id_, extra_field_headers_, extra_fields_);
    }

   private:
    absl::optional<uint8_t> header_;
    absl::optional<std::string> model_id_;
    std::vector<uint8_t> extra_field_headers_;
    std::vector<std::string> extra_fields_;
  };

  FastPairServiceDataCreator(absl::optional<uint8_t> header,
                             absl::optional<std::string> model_id,
                             std::vector<uint8_t> extra_field_headers,
                             std::vector<std::string> extra_fields)
      : header_(header),
        model_id_(model_id),
        extra_field_headers_(extra_field_headers),
        extra_fields_(extra_fields) {}

  std::vector<uint8_t> CreateServiceData() {
    DCHECK_EQ(extra_field_headers_.size(), extra_fields_.size());

    std::vector<uint8_t> service_data;

    if (header_)
      service_data.push_back(header_.value());

    if (model_id_) {
      std::vector<uint8_t> model_id_bytes;
      base::HexStringToBytes(model_id_.value(), &model_id_bytes);
      std::move(std::begin(model_id_bytes), std::end(model_id_bytes),
                std::back_inserter(service_data));
    }

    for (size_t i = 0; i < extra_field_headers_.size(); i++) {
      service_data.push_back(extra_field_headers_[i]);
      std::vector<uint8_t> extra_field_bytes;
      base::HexStringToBytes(extra_fields_[i], &extra_field_bytes);
      std::move(std::begin(extra_field_bytes), std::end(extra_field_bytes),
                std::back_inserter(service_data));
    }

    return service_data;
  }

 private:
  absl::optional<uint8_t> header_;
  absl::optional<std::string> model_id_;
  std::vector<uint8_t> extra_field_headers_;
  std::vector<std::string> extra_fields_;
};

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

TEST_F(FastPairDecoderTest, HasModelId_ThreeByteFormat) {
  EXPECT_TRUE(HasModelIdString(kModelId));
}

TEST_F(FastPairDecoderTest, HasModelId_TooShort) {
  EXPECT_FALSE(HasModelIdString("11"));
}

TEST_F(FastPairDecoderTest, HasModelId_LongFormat) {
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
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00101000)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_FALSE(HasModelId(&bytes));
}

TEST_F(FastPairDecoderTest, HasModelId_LongInvalidLength) {
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

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForNullData) {
  EXPECT_EQ(GetHexModelIdFromServiceData(nullptr), absl::nullopt);
}

TEST_F(FastPairDecoderTest, GetHexModelIdFromServiceData_NoResultForEmptyData) {
  std::vector<uint8_t> empty;
  EXPECT_EQ(GetHexModelIdFromServiceData(&empty), absl::nullopt);
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

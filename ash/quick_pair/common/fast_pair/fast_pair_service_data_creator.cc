// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"

#include <cstdint>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"

namespace ash {
namespace quick_pair {

FastPairServiceDataCreator::Builder::Builder() = default;

FastPairServiceDataCreator::Builder::~Builder() = default;

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::SetHeader(uint8_t byte) {
  header_ = byte;
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::SetModelId(std::string model_id) {
  model_id_ = model_id;
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::AddExtraFieldHeader(uint8_t header) {
  extra_field_headers_.push_back(header);
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::AddExtraField(std::string field) {
  extra_fields_.push_back(field);
  return *this;
}

std::unique_ptr<FastPairServiceDataCreator>
FastPairServiceDataCreator::Builder::Build() {
  return std::make_unique<FastPairServiceDataCreator>(
      header_, model_id_, extra_field_headers_, extra_fields_);
}

FastPairServiceDataCreator::FastPairServiceDataCreator(
    std::optional<uint8_t> header,
    std::optional<std::string> model_id,
    std::vector<uint8_t> extra_field_headers,
    std::vector<std::string> extra_fields)
    : header_(header),
      model_id_(model_id),
      extra_field_headers_(extra_field_headers),
      extra_fields_(extra_fields) {}

FastPairServiceDataCreator::~FastPairServiceDataCreator() = default;

std::vector<uint8_t> FastPairServiceDataCreator::CreateServiceData() {
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

}  // namespace quick_pair
}  // namespace ash

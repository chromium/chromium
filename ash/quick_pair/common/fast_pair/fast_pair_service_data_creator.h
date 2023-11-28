// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_SERVICE_DATA_CREATOR_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_SERVICE_DATA_CREATOR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/strings/string_number_conversions.h"

namespace ash {
namespace quick_pair {

// Convenience class with Builder to create byte arrays which represent Fast
// Pair Service Data.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) FastPairServiceDataCreator {
 public:
  class Builder {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    Builder& SetHeader(uint8_t byte);
    Builder& SetModelId(std::string model_id);
    Builder& AddExtraFieldHeader(uint8_t header);
    Builder& AddExtraField(std::string field);
    std::unique_ptr<FastPairServiceDataCreator> Build();

   private:
    std::optional<uint8_t> header_;
    std::optional<std::string> model_id_;
    std::vector<uint8_t> extra_field_headers_;
    std::vector<std::string> extra_fields_;
  };

  FastPairServiceDataCreator(std::optional<uint8_t> header,
                             std::optional<std::string> model_id,
                             std::vector<uint8_t> extra_field_headers,
                             std::vector<std::string> extra_fields);
  FastPairServiceDataCreator(const FastPairServiceDataCreator&) = delete;
  FastPairServiceDataCreator& operator=(const FastPairServiceDataCreator&) =
      delete;
  ~FastPairServiceDataCreator();

  std::vector<uint8_t> CreateServiceData();

 private:
  std::optional<uint8_t> header_;
  std::optional<std::string> model_id_;
  std::vector<uint8_t> extra_field_headers_;
  std::vector<std::string> extra_fields_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_SERVICE_DATA_CREATOR_H_

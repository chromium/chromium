// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/allocator_config.h"

#include <limits>

#include "partition_alloc/random.h"

namespace partition_alloc {

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE) && \
    PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)
namespace {

enum ExternalMetadataTrialGroupPercentage {
  kEnabled = 25,   // 25% enabled
  kDisabled = 25,  // 25% control
  // Otherwise, disabled (default behavior)
};
// Rather than doing percentage group based assignment, set all clients to
// enabled when true.
constexpr bool kDefaultEnableExternalMetadataTrial = true;

ExternalMetadataTrialGroup s_externalMetadataJoinedGroup =
    ExternalMetadataTrialGroup::kUndefined;

void SetExternalMetadataTrialGroup(ExternalMetadataTrialGroup group) {
  s_externalMetadataJoinedGroup = group;
}

}  // namespace

namespace internal {

ExternalMetadataTrialGroup SelectExternalMetadataTrialGroup() {
  if constexpr (kDefaultEnableExternalMetadataTrial) {
    auto group = ExternalMetadataTrialGroup::kEnabled;
    SetExternalMetadataTrialGroup(group);
    return group;
  }
  uint32_t random = internal::RandomValue() /
                    static_cast<double>(std::numeric_limits<uint32_t>::max()) *
                    100.0;

  ExternalMetadataTrialGroup group;
  if (random < ExternalMetadataTrialGroupPercentage::kEnabled) {
    group = ExternalMetadataTrialGroup::kEnabled;
  } else if (random < ExternalMetadataTrialGroupPercentage::kEnabled +
                          ExternalMetadataTrialGroupPercentage::kDisabled) {
    group = ExternalMetadataTrialGroup::kDisabled;
  } else {
    group = ExternalMetadataTrialGroup::kDefault;
  }
  SetExternalMetadataTrialGroup(group);
  return group;
}

}  // namespace internal

ExternalMetadataTrialGroup GetExternalMetadataTrialGroup() {
  return s_externalMetadataJoinedGroup;
}

#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE) &&
        // PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)

}  // namespace partition_alloc

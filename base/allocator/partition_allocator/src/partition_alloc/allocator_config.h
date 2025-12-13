// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_ALLOCATOR_CONFIG_H_
#define PARTITION_ALLOC_ALLOCATOR_CONFIG_H_

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_config.h"

namespace partition_alloc {

// partition_alloc_support.cc will see the configuration and invoke
// RegisterSyntheticTrialGroup().
#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE) && \
    PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)

inline constexpr const char kExternalMetadataTrialName[] =
    "PartitionAllocExternalMetadata";
inline constexpr const char kExternalMetadataTrialGroup_Enabled[] = "Enabled";
inline constexpr const char kExternalMetadataTrialGroup_Disabled[] = "Disabled";

// For synthetic field trial: PartitionAllocExternalMetadata
enum ExternalMetadataTrialGroup {
  kUndefined = 0,
  kDefault,
  kDisabled,
  kEnabled,
};

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
ExternalMetadataTrialGroup GetExternalMetadataTrialGroup();

namespace internal {

ExternalMetadataTrialGroup SelectExternalMetadataTrialGroup();

}  // namespace internal

#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE) &&
        // PA_BUILDFLAG(ENABLE_MOVE_METADATA_OUT_OF_GIGACAGE_TRIAL)

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_ALLOCATOR_CONFIG_H_

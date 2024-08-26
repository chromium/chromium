// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/feature_list.h"
#include "base/memory/discardable_memory_internal.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/ashmem/ashmem.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {

namespace features {
#if BUILDFLAG(IS_POSIX)
// Feature flag allowing the use of MADV_FREE discardable memory when there are
// multiple supported discardable memory backings.
BASE_FEATURE(kMadvFreeDiscardableMemory,
             "MadvFreeDiscardableMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kDiscardableMemoryBackingTrial,
             "DiscardableMemoryBackingTrial",
             base::FEATURE_DISABLED_BY_DEFAULT);


#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace features

namespace {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

DiscardableMemoryBacking GetBackingForFieldTrial() {
  DiscardableMemoryTrialGroup trial_group =
      GetDiscardableMemoryBackingFieldTrialGroup();
  switch (trial_group) {
    case DiscardableMemoryTrialGroup::kEmulatedSharedMemory:
    case DiscardableMemoryTrialGroup::kAshmem:
      return DiscardableMemoryBacking::kSharedMemory;
    case DiscardableMemoryTrialGroup::kMadvFree:
      return DiscardableMemoryBacking::kMadvFree;
  }
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Probe capabilities of this device to determine whether we should participate
// in the discardable memory backing trial.
bool DiscardableMemoryBackingFieldTrialIsEnabled() {
#if BUILDFLAG(IS_ANDROID)
  if (!ashmem_device_is_supported())
    return false;
#endif  // BUILDFLAG(IS_ANDROID)
  if (base::GetMadvFreeSupport() != base::MadvFreeSupport::kSupported)
    return false;

  // IMPORTANT: Only query the feature after we determine the device has the
  // capabilities required, which will have the side-effect of assigning a
  // trial-group.
  return base::FeatureList::IsEnabled(features::kDiscardableMemoryBackingTrial);
}

DiscardableMemoryTrialGroup GetDiscardableMemoryBackingFieldTrialGroup() {
  DCHECK(DiscardableMemoryBackingFieldTrialIsEnabled());
  return features::kDiscardableMemoryBackingParam.Get();
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

DiscardableMemory::DiscardableMemory() = default;

DiscardableMemory::~DiscardableMemory() = default;

DiscardableMemoryBacking GetDiscardableMemoryBacking() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (DiscardableMemoryBackingFieldTrialIsEnabled()) {
    return GetBackingForFieldTrial();
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  if (ashmem_device_is_supported())
    return DiscardableMemoryBacking::kSharedMemory;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
  if (base::FeatureList::IsEnabled(
          base::features::kMadvFreeDiscardableMemory) &&
      base::GetMadvFreeSupport() == base::MadvFreeSupport::kSupported) {
    return DiscardableMemoryBacking::kMadvFree;
  }
#endif  // BUILDFLAG(IS_POSIX)

  return DiscardableMemoryBacking::kSharedMemory;
}

}  // namespace base

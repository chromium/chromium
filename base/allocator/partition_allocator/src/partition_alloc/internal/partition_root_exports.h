// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_EXPORTS_H_
#define PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_EXPORTS_H_

#include "partition_alloc/partition_alloc_base/component_export.h"

#if DEFINE_PARTITION_ROOT_EXPORT_TEMPLATE
#define EXPORT_TEMPLATE \
  template PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
#else
// Explicitly define common template instantiations to reduce compile time.
#define EXPORT_TEMPLATE                       \
  extern template PA_EXPORT_TEMPLATE_DECLARE( \
      PA_COMPONENT_EXPORT(PARTITION_ALLOC))
#endif

EXPORT_TEMPLATE void* PartitionRoot::AllocInline<AllocFlags::kNone>(
    size_t,
    const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<AllocFlags::kNoHooks>(
    size_t,
    const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<AllocFlags::kZeroFill>(
    size_t,
    const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<
    AllocFlags::kZeroFill | AllocFlags::kReturnNull>(size_t, const char*);
EXPORT_TEMPLATE void*
PartitionRoot::AllocInline<AllocFlags::kNoOverrideHooks |
                           AllocFlags::kNoMemoryToolOverride>(size_t,
                                                              const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<
    AllocFlags::kReturnNull | AllocFlags::kNoOverrideHooks |
    AllocFlags::kNoMemoryToolOverride>(size_t, const char*);
EXPORT_TEMPLATE void*
PartitionRoot::AllocInline<AllocFlags::kReturnNull | AllocFlags::kZeroFill |
                           AllocFlags::kNoOverrideHooks |
                           AllocFlags::kNoMemoryToolOverride>(size_t,
                                                              const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<
    AllocFlags::kZeroFill | AllocFlags::kNoOverrideHooks |
    AllocFlags::kNoMemoryToolOverride>(size_t, const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<AllocFlags::kReturnNull>(
    size_t,
    const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<
    AllocFlags::kReturnNull | AllocFlags::kNoHooks>(size_t, const char*);
EXPORT_TEMPLATE void* PartitionRoot::AllocInline<
    AllocFlags::kZeroFill | AllocFlags::kNoHooks>(size_t, const char*);
EXPORT_TEMPLATE void*
PartitionRoot::AllocInline<AllocFlags::kReturnNull | AllocFlags::kZeroFill |
                           AllocFlags::kNoHooks>(size_t, const char*);
EXPORT_TEMPLATE void*
PartitionRoot::AllocInline<AllocFlags::kFastPathOrReturnNull>(size_t,
                                                              const char*);
EXPORT_TEMPLATE void*
PartitionRoot::AllocInline<AllocFlags::kNoHooks | AllocFlags::kNoOverrideHooks |
                           AllocFlags::kNoMemoryToolOverride>(size_t,
                                                              const char*);

EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kNone, FreeFlags::kNone>(void*,
                                                            size_t,
                                                            const char*);
EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kReturnNull, FreeFlags::kNone>(void*,
                                                                  size_t,
                                                                  const char*);
EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kNoHooks, FreeFlags::kNoHooks>(void*,
                                                                  size_t,
                                                                  const char*);
EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kReturnNull | AllocFlags::kNoHooks,
                       FreeFlags::kNone>(void*, size_t, const char*);
EXPORT_TEMPLATE void* PartitionRoot::Realloc<
    AllocFlags::kNoHooks,
    FreeFlags::kNoHooks | FreeFlags::kSchedulerLoopQuarantine>(void*,
                                                               size_t,
                                                               const char*);

EXPORT_TEMPLATE void* PartitionRoot::AlignedAlloc<AllocFlags::kNone>(size_t,
                                                                     size_t);
EXPORT_TEMPLATE void* PartitionRoot::AlignedAlloc<
    AllocFlags::kReturnNull | AllocFlags::kZeroFill>(size_t, size_t);
EXPORT_TEMPLATE void* PartitionRoot::AlignedAlloc<AllocFlags::kNoHooks>(size_t,
                                                                        size_t);
EXPORT_TEMPLATE void* PartitionRoot::AlignedAlloc<AllocFlags::kReturnNull |
                                                  AllocFlags::kNoHooks>(size_t,
                                                                        size_t);

EXPORT_TEMPLATE void PartitionRoot::FreeInline<FreeFlags::kNone>(void*);
EXPORT_TEMPLATE void
PartitionRoot::FreeInline<FreeFlags::kNoMemoryToolOverride>(void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<
    FreeFlags::kNoMemoryToolOverride | FreeFlags::kNoHooks>(void*);
EXPORT_TEMPLATE void
PartitionRoot::FreeInline<FreeFlags::kSchedulerLoopQuarantine>(void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<FreeFlags::kNoHooks>(void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<
    FreeFlags::kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks>(void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<FreeFlags::kIntendedLeak>(void*);
EXPORT_TEMPLATE
void PartitionRoot::FreeInline<FreeFlags::kWithSizeHint>(
    void*,
    FreeHintType<FreeFlags::kWithSizeHint>);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<FreeFlags::kWithSizeHint |
                                               FreeFlags::kWithAlignmentHint>(
    void*,
    FreeHintType<FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint>);
EXPORT_TEMPLATE void PartitionRoot::FreeInline<FreeFlags::kIntendedLeak |
                                               FreeFlags::kWithTypeIdHint>(
    void*,
    FreeHintType<FreeFlags::kWithTypeIdHint>);

EXPORT_TEMPLATE void PartitionRoot::AlignedFree<FreeFlags::kNone>(void*);

EXPORT_TEMPLATE void PartitionRoot::FreeInUnknownRoot<FreeFlags::kNone>(void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInUnknownRoot<FreeFlags::kNoHooks>(
    void*);
EXPORT_TEMPLATE void PartitionRoot::FreeInUnknownRoot<
    FreeFlags::kNoHooks | FreeFlags::kSchedulerLoopQuarantine>(void*);

EXPORT_TEMPLATE
void PartitionRoot::FreeInUnknownRoot<FreeFlags::kNoHooks |
                                      FreeFlags::kWithSizeHint>(
    void*,
    FreeHintType<FreeFlags::kWithSizeHint>);
EXPORT_TEMPLATE
void PartitionRoot::FreeInUnknownRoot<
    FreeFlags::kNoHooks | FreeFlags::kSchedulerLoopQuarantine |
    FreeFlags::kWithSizeHint>(void*, FreeHintType<FreeFlags::kWithSizeHint>);
EXPORT_TEMPLATE
void PartitionRoot::FreeInUnknownRoot<FreeFlags::kNoHooks |
                                      FreeFlags::kWithSizeHint |
                                      FreeFlags::kWithAlignmentHint>(
    void* object,
    FreeHintType<FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint>);
EXPORT_TEMPLATE
void PartitionRoot::FreeInUnknownRoot<
    FreeFlags::kNoHooks | FreeFlags::kSchedulerLoopQuarantine |
    FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint>(
    void* object,
    FreeHintType<FreeFlags::kWithSizeHint | FreeFlags::kWithAlignmentHint>);

#undef EXPORT_TEMPLATE

#endif  // PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_EXPORTS_H_

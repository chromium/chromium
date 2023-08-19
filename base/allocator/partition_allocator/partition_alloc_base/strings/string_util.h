// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base::strings {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
const char* FindLastOf(const char* text, const char* characters);
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
const char* FindLastNotOf(const char* text, const char* characters);

}  // namespace partition_alloc::internal::base::strings

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_

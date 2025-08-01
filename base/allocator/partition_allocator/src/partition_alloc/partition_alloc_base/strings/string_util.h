// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_

#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base::strings {

PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
const char* FindLastOf(const char* text, const char* characters);
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
const char* FindLastNotOf(const char* text, const char* characters);

}  // namespace partition_alloc::internal::base::strings

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_STRINGS_STRING_UTIL_H_

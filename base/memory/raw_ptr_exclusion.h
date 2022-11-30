// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_EXCLUSION_H_
#define BASE_MEMORY_RAW_PTR_EXCLUSION_H_

#include "base/allocator/buildflags.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if HAS_ATTRIBUTE(annotate)
#if defined(OFFICIAL_BUILD) && !BUILDFLAG(FORCE_ENABLE_RAW_PTR_EXCLUSION)
// The annotation changed compiler output and increased binary size so disable
// for official builds.
// TODO(crbug.com/1320670): Remove when issue is resolved.
#define RAW_PTR_EXCLUSION
#else
// Marks a field as excluded from the raw_ptr usage enforcement clang plugin.
// Example: RAW_PTR_EXCLUSION Foo* foo_;
#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))
#endif
#else
#define RAW_PTR_EXCLUSION
#endif

#endif  // BASE_MEMORY_RAW_PTR_EXCLUSION_H_

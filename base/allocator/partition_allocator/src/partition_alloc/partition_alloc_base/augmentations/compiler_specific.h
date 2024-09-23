// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_AUGMENTATIONS_COMPILER_SPECIFIC_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_AUGMENTATIONS_COMPILER_SPECIFIC_H_

// Extensions for PA's copy of `//base/compiler_specific.h`.

#include "partition_alloc/partition_alloc_base/compiler_specific.h"

// PA_ATTRIBUTE_RETURNS_NONNULL
//
// Tells the compiler that a function never returns a null pointer.
// Sourced from Abseil's `attributes.h`.
#if PA_HAS_ATTRIBUTE(returns_nonnull)
#define PA_ATTRIBUTE_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define PA_ATTRIBUTE_RETURNS_NONNULL
#endif

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_AUGMENTATIONS_COMPILER_SPECIFIC_H_

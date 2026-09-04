// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_LINUX_NAME_REGION_H_
#define PARTITION_ALLOC_LINUX_NAME_REGION_H_

#include "partition_alloc/build_config.h"

// Ability to name anonymous VMAs is available on some, but not all Linux-based
// systems.
#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) || \
    PA_BUILDFLAG(IS_CHROMEOS)
#include <sys/prctl.h>

#if (PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)) && \
    !(defined(PR_SET_VMA) && defined(PR_SET_VMA_ANON_NAME))

// The PR_SET_VMA* symbols are originally from
// https://android.googlesource.com/platform/bionic/+/lollipop-release/libc/private/bionic_prctl.h
// and were subsequently added to mainline Linux in Jan 2022, see
// https://github.com/torvalds/linux/commit/9a10064f5625d5572c3626c1516e0bebc6c9fe9b.
//
// Define them to support compiling with older headers.
#if !defined(PR_SET_VMA)
#define PR_SET_VMA 0x53564d41
#endif

#if !defined(PR_SET_VMA_ANON_NAME)
#define PR_SET_VMA_ANON_NAME 0
#endif

#endif  // (PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)) &&
        // !(defined(PR_SET_VMA) && defined(PR_SET_VMA_ANON_NAME))

#if defined(PR_SET_VMA) && defined(PR_SET_VMA_ANON_NAME)
#define LINUX_NAME_REGION 1
#endif

#endif  // PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_LINUX) ||
        // PA_BUILDFLAG(IS_CHROMEOS)

#endif  // PARTITION_ALLOC_LINUX_NAME_REGION_H_

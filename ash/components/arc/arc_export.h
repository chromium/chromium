// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ARC_EXPORT_H_
#define ASH_COMPONENTS_ARC_ARC_EXPORT_H_

#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error "ARC can be built only for Chrome OS."
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(COMPONENT_BUILD) && defined(ARC_IMPLEMENTATION)
#define ARC_EXPORT __attribute__((visibility("default")))
#else  // !defined(COMPONENT_BUILD) || !defined(ARC_IMPLEMENTATION)
#define ARC_EXPORT
#endif

#endif  // ASH_COMPONENTS_ARC_ARC_EXPORT_H_

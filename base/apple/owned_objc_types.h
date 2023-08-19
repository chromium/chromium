// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards; it's included inside
// macros to generate classes. The following lines silence a presubmit and
// Tricium warning that would otherwise be triggered by this:
//
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#if !defined(BASE_APPLE_OWNED_OBJC_H_)
#error Please #include "base/apple/owned_objc.h" instead of this file
#endif

#if BUILDFLAG(IS_MAC)
GENERATE_STRONG_OBJC_TYPE(NSCursor)
GENERATE_STRONG_OBJC_TYPE(NSEvent)
#elif BUILDFLAG(IS_IOS)
GENERATE_STRONG_OBJC_TYPE(UIEvent)
#endif

#if BUILDFLAG(IS_MAC)
GENERATE_WEAK_OBJC_TYPE(NSView)
GENERATE_WEAK_OBJC_TYPE(NSWindow)
#elif BUILDFLAG(IS_IOS)
GENERATE_WEAK_OBJC_TYPE(UIView)
GENERATE_WEAK_OBJC_TYPE(UIWindow)
#endif

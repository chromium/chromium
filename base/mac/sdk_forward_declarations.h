// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built.

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#include <AvailabilityMacros.h>
#include <AvailabilityVersions.h>
#include <os/availability.h>

// NOTE: If an #import is needed only for a newer SDK, it might be found below.

#include "base/base_export.h"

// ----------------------------------------------------------------------------
// Definitions from SDKs newer than the one that Chromium compiles against.
//
// HOW TO DO THIS:
//
// 1. In this file:
//   a. Use an #if !defined() guard
//   b. Include all API_AVAILABLE/NS_CLASS_AVAILABLE_MAC annotations
//   c. Optionally import frameworks
// 2. In your source file:
//   a. Correctly annotate availability with @available/__builtin_available/
//      API_AVAILABLE
//
// This way, when the SDK is rolled, the section full of definitions
// corresponding to it can be easily deleted.
//
// EXAMPLES OF HOW TO DO THIS:
//
// Suppose there's a simple extension of NSApplication in macOS 10.25. Then:
//
//   #if !defined(MAC_OS_X_VERSION_10_25) || \
//       MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_25
//
//   @interface NSApplication (MacOSHouseCatSDK)
//   @property(readonly) CGFloat purrRate API_AVAILABLE(macos(10.25));
//   @end
//
//   #endif  // MAC_OS_X_VERSION_10_25
//
//
// Suppose the CoreShoelace framework is introduced in macOS 10.77. Then:
//
//   #if !defined(MAC_OS_X_VERSION_10_77) || \
//       MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_77
//
//   API_AVAILABLE(macos(10.77))
//   @interface NSCoreShoelace : NSObject
//   @property (readonly) NSUInteger stringLength;
//   @end
//
//   #else
//
//   #import <CoreShoelace/CoreShoelace.h>
//
//   #endif  // MAC_OS_X_VERSION_10_77
//
// ----------------------------------------------------------------------------

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

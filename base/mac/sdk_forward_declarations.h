// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built.

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#import <AppKit/AppKit.h>
#include <AvailabilityMacros.h>
#include <os/availability.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

// NOTE: If an #import is needed only for a newer SDK, it might be found below.

#include "base/base_export.h"

// Once Chrome no longer supports OSX 10.13.4, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_13_4) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_13_4

API_AVAILABLE(macos(10.13.4))
@interface  MPSCNNArithmetic : MPSCNNBinaryKernel
@end

API_AVAILABLE(macos(10.13.4))
@interface  MPSCNNAdd : MPSCNNArithmetic
@end

API_AVAILABLE(macos(10.13.4))
@interface  MPSCNNMultiply : MPSCNNArithmetic
@end

#endif  // MAC_OS_X_VERSION_10_13_4

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

// Chromium currently is building with the most recent SDK. WWDC is not far
// away, though....

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

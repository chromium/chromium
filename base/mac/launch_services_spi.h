// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_LAUNCH_SERVICES_SPI_H_
#define BASE_MAC_LAUNCH_SERVICES_SPI_H_

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>

// Private SPIs exposed by LaunchServices. Largely derived from usage of these
// in open source WebKit code and some inspection of the LaunchServices binary.

extern "C" {

using LSASNRef = const struct CF_BRIDGED_TYPE(id) __LSASN*;

extern const CFStringRef _kLSOpenOptionActivateKey;
extern const CFStringRef _kLSOpenOptionAddToRecentsKey;
extern const CFStringRef _kLSOpenOptionArgumentsKey;
extern const CFStringRef _kLSOpenOptionBackgroundLaunchKey;
extern const CFStringRef _kLSOpenOptionHideKey;
extern const CFStringRef _kLSOpenOptionPreferRunningInstanceKey;

using _LSOpenCompletionHandler = void (^)(LSASNRef, Boolean, CFErrorRef);
void _LSOpenURLsWithCompletionHandler(
    CFArrayRef urls,
    CFURLRef application_url,
    CFDictionaryRef options,
    _LSOpenCompletionHandler completion_handler);

@interface NSRunningApplication ()
- (id)initWithApplicationSerialNumber:(LSASNRef)asn;
@end

}  // extern "C"

#endif  // BASE_MAC_LAUNCH_SERVICES_SPI_H_

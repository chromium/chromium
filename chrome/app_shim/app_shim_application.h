// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_APPLICATION_H_
#define CHROME_APP_SHIM_APP_SHIM_APPLICATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_pump_apple.h"
#include "chrome/common/mac/app_mode_common.h"

// The NSApplication for app shims is a vanilla NSApplication, but
// implements the CrAppProtocol and CrAppControlPrototocol protocols to skip
// creating an autorelease pool in nested event loops, for example when
// displaying a context menu.
@interface AppShimApplication
    : NSApplication <CrAppProtocol, CrAppControlProtocol>
@end

#endif  // CHROME_APP_SHIM_APP_SHIM_APPLICATION_H_

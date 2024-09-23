// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#include "chrome/common/mac/app_shim.mojom.h"

class AppShimController;

// An application delegate to catch user interactions and send the appropriate
// IPC messages to Chrome.
@interface AppShimDelegate
    : NSObject <NSApplicationDelegate, NSUserInterfaceValidations>
- (instancetype)initWithController:(AppShimController*)controller;
- (void)enableAccessibilitySupport:
    (chrome::mojom::AppShimScreenReaderSupportMode)mode;
@end

#endif  // CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_CENTER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_CENTER_MAC_H_

#import <Foundation/Foundation.h>

// Stubs NSUserNotificationCenter so it can be used in tests without actually
// displaying notifications.
// Unlike the real class this is not a singleton and the lifecycle needs to be
// handled by the caller.
// See notification_platform_bridge_mac_unittest.mm for an example.
@interface StubNotificationCenter : NSUserNotificationCenter
- (instancetype _Nullable)init;
- (void)setDelegate:(id<NSUserNotificationCenterDelegate> _Nullable)delegate;
- (id<NSUserNotificationCenterDelegate> _Nullable)delegate;
@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_CENTER_MAC_H_

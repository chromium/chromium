// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_application.h"

#include "base/apple/foundation_util.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/common/mac/app_shim.mojom.h"

@implementation AppShimApplication {
  BOOL _handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (void)enableScreenReaderCompleteModeAfterDelay:(BOOL)enable {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector
                                           (enableScreenReaderCompleteMode)
                                             object:nil];
  if (enable) {
    const float kTwoSecondDelay = 2.0;
    [self performSelector:@selector(enableScreenReaderCompleteMode)
               withObject:nil
               afterDelay:kTwoSecondDelay];
  }
}

- (void)enableScreenReaderCompleteMode {
  AppShimDelegate* delegate =
      base::apple::ObjCCastStrict<AppShimDelegate>(NSApp.delegate);
  [delegate enableAccessibilitySupport:
                chrome::mojom::AppShimScreenReaderSupportMode::kComplete];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  // This is an undocumented attribute that's set when VoiceOver is turned
  // on/off. We track VoiceOver state changes using KVO, but monitor this
  // attribute in case other ATs use it to request accessibility activation.
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    // When there are ATs that want to access this PWA's accessibility, we
    // need to notify the browser process to enable accessibility. When ATs no
    // longer need access to this PWA's accessibility, we don't want it to
    // affect the browser in case other PWA apps or the browser itself still
    // need to use accessbility.
    if ([value boolValue]) {
      [self enableScreenReaderCompleteModeAfterDelay:YES];
    }
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}

- (BOOL)sendAction:(SEL)anAction to:(id)aTarget from:(id)sender {
  // The Dock menu contains an automagic section where you can select
  // amongst open windows.  If a window is closed via JavaScript while
  // the menu is up, the menu item for that window continues to exist.
  // When a window is selected this method is called with the
  // now-freed window as |aTarget|.  Short-circuit the call if
  // |aTarget| is not a valid window.
  if (anAction == @selector(_selectWindow:)) {
    // Not using -[NSArray containsObject:] because |aTarget| may be a
    // freed object.
    BOOL found = NO;
    for (NSWindow* window in [self windows]) {
      if (window == aTarget) {
        found = YES;
        break;
      }
    }
    if (!found) {
      return NO;
    }
  }

  return [super sendAction:anAction to:aTarget from:sender];
}

- (NSAccessibilityRole)accessibilityRole {
  AppShimDelegate* delegate =
      base::apple::ObjCCastStrict<AppShimDelegate>(NSApp.delegate);
  [delegate enableAccessibilitySupport:
                chrome::mojom::AppShimScreenReaderSupportMode::kPartial];
  return [super accessibilityRole];
}

@end

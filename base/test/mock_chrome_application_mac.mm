// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_chrome_application_mac.h"

#include "base/auto_reset.h"
#include "base/check.h"

@implementation MockCrApp

+ (NSApplication*)sharedApplication {
  NSApplication* app = [super sharedApplication];
  DCHECK([app conformsToProtocol:@protocol(CrAppControlProtocol)])
      << "Existing NSApp (class " << [[app className] UTF8String]
      << ") does not conform to required protocol.";
  DCHECK(base::message_pump_apple::UsingCrApp())
      << "message_pump_apple::Create() was called before "
      << "+[MockCrApp sharedApplication]";
  return app;
}

- (void)sendEvent:(NSEvent*)event {
  base::AutoReset<BOOL> scoper(&_handlingSendEvent, YES);
  [super sendEvent:event];
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

@end

namespace mock_cr_app {

void RegisterMockCrApp() {
  [MockCrApp sharedApplication];

  // If there was an invocation to NSApp prior to this method, then the NSApp
  // will not be a MockCrApp, but will instead be an NSApplication.
  // This is undesirable and we must enforce that this doesn't happen.
  CHECK([NSApp isKindOfClass:[MockCrApp class]]);
}

}  // namespace mock_cr_app

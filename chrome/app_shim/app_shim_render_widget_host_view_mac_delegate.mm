// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app_shim/app_shim_render_widget_host_view_mac_delegate.h"

#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"

@interface AppShimRenderWidgetHostViewMacDelegate () <HistorySwiperDelegate>
@end

@implementation AppShimRenderWidgetHostViewMacDelegate {
  uint64_t _nsviewIDThatWantsHistoryOverlay;

  // Responsible for 2-finger swipes history navigation.
  HistorySwiper* __strong _historySwiper;
}

- (instancetype)initWithRenderWidgetHostNSViewID:
    (uint64_t)renderWidgetHostNSViewID {
  if (self = [super init]) {
    _nsviewIDThatWantsHistoryOverlay = renderWidgetHostNSViewID;
    _historySwiper = [[HistorySwiper alloc] initWithDelegate:self];
  }
  return self;
}

- (void)dealloc {
  _historySwiper.delegate = nil;
}

// Handle an event. All incoming key and mouse events flow through this
// delegate method if implemented. Return YES if the event is fully handled, or
// NO if normal processing should take place.
- (BOOL)handleEvent:(NSEvent*)event {
  return [_historySwiper handleEvent:event];
}

// NSWindow events.

- (void)beginGestureWithEvent:(NSEvent*)event {
  [_historySwiper beginGestureWithEvent:event];
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [_historySwiper endGestureWithEvent:event];
}

// This is a low level API which provides touches associated with an event.
// It is used in conjunction with gestures to determine finger placement
// on the trackpad.
- (void)touchesMovedWithEvent:(NSEvent*)event {
  [_historySwiper touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [_historySwiper touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [_historySwiper touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [_historySwiper touchesEndedWithEvent:event];
}

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  [_historySwiper rendererHandledWheelEvent:event consumed:consumed];
}

- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  [_historySwiper rendererHandledGestureScrollEvent:event consumed:consumed];
}

- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params {
  [_historySwiper onOverscrolled:params];
}

// HistorySwiperDelegate methods.

- (BOOL)shouldAllowHistorySwiping {
  return YES;
}

- (NSView*)viewThatWantsHistoryOverlay {
  return remote_cocoa::GetNSViewFromId(_nsviewIDThatWantsHistoryOverlay);
}

- (BOOL)canNavigateInDirection:(history_swiper::NavigationDirection)direction
                      onWindow:(NSWindow*)window {
  auto* bridge =
      remote_cocoa::NativeWidgetNSWindowBridge::GetFromNativeWindow(window);
  if (!bridge)
    return NO;

  if (direction == history_swiper::kForwards) {
    return bridge->CanGoForward();
  } else {
    return bridge->CanGoBack();
  }
}

- (void)navigateInDirection:(history_swiper::NavigationDirection)direction
                   onWindow:(NSWindow*)window {
  auto* bridge =
      remote_cocoa::NativeWidgetNSWindowBridge::GetFromNativeWindow(window);
  if (!bridge)
    return;

  bool was_executed = false;
  if (direction == history_swiper::kForwards) {
    bridge->host()->ExecuteCommand(
        IDC_FORWARD, WindowOpenDisposition::CURRENT_TAB, false, &was_executed);
  } else {
    bridge->host()->ExecuteCommand(IDC_BACK, WindowOpenDisposition::CURRENT_TAB,
                                   false, &was_executed);
  }
  DCHECK(was_executed);
}

- (void)backwardsSwipeNavigationLikely {
  // TODO(mcnee): It's unclear whether preloading predictions would be useful in
  // this context. For now we don't do any prediction. See
  // https://crbug.com/1422266 for context.
}

@end

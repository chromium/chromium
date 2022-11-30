// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"

@class HistorySwiper;
@interface AppShimRenderWidgetHostViewMacDelegate
    : NSObject <RenderWidgetHostViewMacDelegate> {
 @private
  uint64_t _nsviewIDThatWantsHistoryOverlay;

  // Responsible for 2-finger swipes history navigation.
  base::scoped_nsobject<HistorySwiper> _historySwiper;
}

- (instancetype)initWithRenderWidgetHostNSViewID:
    (uint64_t)renderWidgetHostNSViewID;

@end

#endif  // CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

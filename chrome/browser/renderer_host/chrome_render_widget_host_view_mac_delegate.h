// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"

namespace content {
class RenderWidgetHost;
}

@class HistorySwiper;
@interface ChromeRenderWidgetHostViewMacDelegate
    : NSObject<RenderWidgetHostViewMacDelegate> {
 @private
  raw_ptr<content::RenderWidgetHost> _renderWidgetHost;  // weak

  // Responsible for 2-finger swipes history navigation.
  base::scoped_nsobject<HistorySwiper> _historySwiper;
}

- (instancetype)initWithRenderWidgetHost:
    (content::RenderWidgetHost*)renderWidgetHost;

- (BOOL)handleEvent:(NSEvent*)event;
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid;
@end

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

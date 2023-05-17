// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#import <Cocoa/Cocoa.h>

#import "content/public/browser/render_widget_host_view_mac_delegate.h"

@class HistorySwiper;
@interface AppShimRenderWidgetHostViewMacDelegate
    : NSObject <RenderWidgetHostViewMacDelegate>

- (instancetype)initWithRenderWidgetHostNSViewID:
    (uint64_t)renderWidgetHostNSViewID;

@end

#endif  // CHROME_APP_SHIM_APP_SHIM_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

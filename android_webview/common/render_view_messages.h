// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "android_webview/common/aw_hit_test_data.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

// Singly-included section for enums and custom IPC traits.
#ifndef ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_
#define ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_

namespace IPC {

// TODO - add enums and custom IPC traits here when needed.

}  // namespace IPC

#endif  // ANDROID_WEBVIEW_COMMON_RENDER_VIEW_MESSAGES_H_

IPC_STRUCT_TRAITS_BEGIN(android_webview::AwHitTestData)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(extra_data_for_type)
  IPC_STRUCT_TRAITS_MEMBER(href)
  IPC_STRUCT_TRAITS_MEMBER(anchor_text)
  IPC_STRUCT_TRAITS_MEMBER(img_src)
IPC_STRUCT_TRAITS_END()

#define IPC_MESSAGE_START AndroidWebViewMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to drop all WebCore memory cache.
IPC_MESSAGE_CONTROL0(AwViewMsg_ClearCache)

// Tells render process to kill itself (only for testing).
IPC_MESSAGE_CONTROL0(AwViewMsg_KillProcess)

// Request for the renderer to determine if the document contains any image
// elements.  The id should be passed in the response message so the response
// can be associated with the request.
IPC_MESSAGE_ROUTED1(AwViewMsg_DocumentHasImages,
                    uint32_t /* id */)

// Do hit test at the given webview coordinate. "Webview" coordinates are
// physical pixel values with the 0,0 at the top left of the current displayed
// view (ie 0,0 is not the top left of the page if the page is scrolled).
IPC_MESSAGE_ROUTED2(AwViewMsg_DoHitTest,
                    gfx::PointF /* touch_center */,
                    gfx::SizeF /* touch_area */)

// Sets the zoom factor for text only. Used in layout modes other than
// Text Autosizing.
IPC_MESSAGE_ROUTED1(AwViewMsg_SetTextZoomFactor,
                    float /* zoom_factor */)

// Resets WebKit WebView scrolling and scale state. We need to send this
// message whenever we want to guarantee that page's scale will be
// recalculated by WebKit.
IPC_MESSAGE_ROUTED0(AwViewMsg_ResetScrollAndScaleState)

// Sets the initial page scale. This overrides initial scale set by
// the meta viewport tag.
IPC_MESSAGE_ROUTED1(AwViewMsg_SetInitialPageScale,
                    double /* page_scale_factor */)

// Sets the base background color for this view.
IPC_MESSAGE_ROUTED1(AwViewMsg_SetBackgroundColor,
                    SkColor)

IPC_MESSAGE_CONTROL1(AwViewMsg_SetJsOnlineProperty,
                     bool /* network_up */)

// Tells blink to smooth scroll to the specified location within |duration_ms|.
IPC_MESSAGE_ROUTED3(AwViewMsg_SmoothScroll,
                    int /* target_x */,
                    int /* target_y */,
                    base::TimeDelta /* duration */)

// Sent to inform renderers whether the internal error page should be shown or
// not.
IPC_MESSAGE_ROUTED1(AwViewMsg_WillSuppressErrorPage,
                    bool /* will_suppress_error_page */)

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the renderer to the browser process.

// Response to AwViewMsg_DocumentHasImages request.
IPC_MESSAGE_ROUTED2(AwViewHostMsg_DocumentHasImagesResponse,
                    int, /* id */
                    bool /* has_images */)

// Response to AwViewMsg_DoHitTest.
IPC_MESSAGE_ROUTED1(AwViewHostMsg_UpdateHitTestData,
                    android_webview::AwHitTestData)

// Sent whenever the contents size (as seen by RenderView) is changed.
IPC_MESSAGE_ROUTED1(AwViewHostMsg_OnContentsSizeChanged,
                    gfx::Size /* contents_size */)

// Sent immediately before a top level navigation is initiated within Blink.
// There are some exlusions, the most important ones are it is not sent
// when creating a popup window, and not sent for application initiated
// navigations. See AwContentRendererClient::HandleNavigation for all
// cornercases. This is sent before updating the NavigationController state
// or creating a URLRequest for the main frame resource.
IPC_SYNC_MESSAGE_CONTROL5_1(AwViewHostMsg_ShouldOverrideUrlLoading,
                            int /* render_frame_id id */,
                            base::string16 /* in - url */,
                            bool /* in - has_user_gesture */,
                            bool /* in - is_redirect */,
                            bool /* in - is_main_frame */,
                            bool /* out - result */)

// Sent when a subframe is created.
IPC_MESSAGE_CONTROL2(AwViewHostMsg_SubFrameCreated,
                     int /* parent_render_frame_id */,
                     int /* child_render_frame_id */)

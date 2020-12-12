// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#define IPC_MESSAGE_START AndroidWebViewMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the renderer to the browser process.

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

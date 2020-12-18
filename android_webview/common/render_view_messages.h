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

// Sent when a subframe is created.
IPC_MESSAGE_CONTROL2(AwViewHostMsg_SubFrameCreated,
                     int /* parent_render_frame_id */,
                     int /* child_render_frame_id */)

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace android_webview {

class ChildFrame;

struct ParentCompositorDrawConstraints {
  gfx::Size viewport_size;
  gfx::Transform transform;

  ParentCompositorDrawConstraints();
  ParentCompositorDrawConstraints(const gfx::Size& viewport_size,
                                  const gfx::Transform& transform);
  bool NeedUpdate(const ChildFrame& frame) const;

  bool operator==(const ParentCompositorDrawConstraints& other) const;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_

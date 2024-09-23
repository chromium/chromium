// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_CLIENT_H_

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {
class TouchHandleDrawable;
}

namespace android_webview {

class BrowserViewRendererClient {
 public:

  // Called when a new Picture is available. Needs to be enabled
  // via the EnableOnNewPicture method.
  virtual void OnNewPicture() = 0;

  // Called to trigger view invalidations.
  // This calls postInvalidateOnAnimation if outside of a vsync, otherwise it
  // calls invalidate.
  virtual void PostInvalidate(bool inside_vsync) = 0;

  // Called to get view's absolute location on the screen.
  virtual gfx::Point GetLocationOnScreen() = 0;

  // Try to set the view's scroll offset to |new_value|.
  virtual void ScrollContainerViewTo(const gfx::Point& new_value) = 0;

  // Sets the following:
  // view's scroll offset cap to |max_scroll_offset|,
  // current contents_size to |contents_size_dip|,
  // the current page scale to |page_scale_factor| and page scale limits
  // to |min_page_scale_factor|..|max_page_scale_factor|.
  virtual void UpdateScrollState(const gfx::Point& max_scroll_offset,
                                 const gfx::SizeF& contents_size_dip,
                                 float page_scale_factor,
                                 float min_page_scale_factor,
                                 float max_page_scale_factor) = 0;

  // Handle overscroll.
  virtual void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                             const gfx::Vector2dF& overscroll_velocity,
                             bool inside_vsync) = 0;

  // Create a text selection handle on demand.
  virtual ui::TouchHandleDrawable* CreateDrawable() = 0;

  // Called when the view tree force dark state changes
  virtual void OnViewTreeForceDarkStateChanged(
      bool view_tree_force_dark_state) = 0;

  virtual void SetPreferredFrameInterval(
      base::TimeDelta preferred_frame_interval) = 0;

 protected:
  virtual ~BrowserViewRendererClient() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_BROWSER_VIEW_RENDERER_CLIENT_H_

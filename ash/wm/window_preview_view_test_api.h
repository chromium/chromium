// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PREVIEW_VIEW_TEST_API_H_
#define ASH_WM_WINDOW_PREVIEW_VIEW_TEST_API_H_

#include "ash/wm/window_mirror_view.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect_f.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class WindowPreviewView;

// Use the api in this class to test WindowPreviewView.
class WindowPreviewViewTestApi {
 public:
  explicit WindowPreviewViewTestApi(WindowPreviewView* preview_view);
  ~WindowPreviewViewTestApi();

  gfx::RectF GetUnionRect() const;

  const base::flat_map<aura::Window*, WindowMirrorView*>& GetMirrorViews()
      const;

  // Gets the mirror view in |mirror_views_| associated with |widget|. Returns
  // null if |widget|'s window does not exist in |mirror_views_|.
  WindowMirrorView* GetMirrorViewForWidget(views::Widget* widget);

 private:
  WindowPreviewView* preview_view_;

  DISALLOW_COPY_AND_ASSIGN(WindowPreviewViewTestApi);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_PREVIEW_VIEW_TEST_API_H_

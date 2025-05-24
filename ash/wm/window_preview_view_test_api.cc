// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_preview_view_test_api.h"

#include "ash/wm/window_preview_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

WindowPreviewViewTestApi::WindowPreviewViewTestApi(
    WindowPreviewView* preview_view)
    : preview_view_(preview_view) {
  DCHECK(preview_view);
}

WindowPreviewViewTestApi::~WindowPreviewViewTestApi() = default;

gfx::RectF WindowPreviewViewTestApi::GetUnionRect() const {
  return preview_view_->GetUnionRect();
}

const base::flat_map<aura::Window*, raw_ptr<WindowMirrorView, CtnExperimental>>&
WindowPreviewViewTestApi::GetMirrorViews() const {
  return preview_view_->mirror_views_;
}

WindowMirrorView* WindowPreviewViewTestApi::GetMirrorViewForWidget(
    views::Widget* widget) {
  auto it = preview_view_->mirror_views_.find(widget->GetNativeWindow());
  if (it != preview_view_->mirror_views_.end())
    return it->second;

  return nullptr;
}

}  // namespace ash

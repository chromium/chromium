// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/view_tree_host_widget.h"

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ui/compositor/layer_type.h"

namespace ash {
namespace {

class ViewTreeHostWidget : public views::Widget {
 public:
  ViewTreeHostWidget() = default;
  ~ViewTreeHostWidget() override = default;

  ViewTreeHostWidget(const ViewTreeHostWidget&) = delete;
  ViewTreeHostWidget& operator=(const ViewTreeHostWidget&) = delete;

  // views::Widget:
  views::internal::RootView* CreateRootView() override {
    return new ViewTreeHostRootView(this);
  }
  void SchedulePaintInRect(const gfx::Rect& rect) override {
    static_cast<ViewTreeHostRootView*>(GetRootView())
        ->SchedulePaintInRect(rect);
  }
};

}  // namespace

views::Widget* CreateViewTreeHostWidget(views::Widget::InitParams params) {
  views::Widget* widget = new ViewTreeHostWidget();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // ViewTreeHostWidget shouldn't use shadow.
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  widget->Init(std::move(params));
  return widget;
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_widget_builder.h"

#include "ash/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// WidgetDelegate that is resizable and creates ash's NonClientFrameView
// implementation.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {
    SetCanFullscreen(true);
    SetCanMaximize(true);
    SetCanMinimize(true);
    SetCanResize(true);
  }
  TestWidgetDelegate(const TestWidgetDelegate& other) = delete;
  TestWidgetDelegate& operator=(const TestWidgetDelegate& other) = delete;
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return Shell::Get()->CreateDefaultNonClientFrameView(widget);
  }
};

}  // namespace

TestWidgetBuilder::TestWidgetBuilder() = default;

TestWidgetBuilder::~TestWidgetBuilder() = default;

TestWidgetBuilder& TestWidgetBuilder::SetWidgetType(
    views::Widget::InitParams::Type type) {
  DCHECK(!built_);
  widget_init_params_.type = type;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetZOrderLevel(ui::ZOrderLevel z_order) {
  DCHECK(!built_);
  widget_init_params_.z_order = z_order;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetDelegate(
    views::WidgetDelegate* delegate) {
  DCHECK(!built_);
  widget_init_params_.delegate = delegate;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  widget_init_params_.bounds = bounds;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetParent(aura::Window* parent) {
  DCHECK(!built_);
  widget_init_params_.parent = parent;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetContext(aura::Window* context) {
  DCHECK(!built_);
  widget_init_params_.context = context;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetActivatable(bool activatable) {
  DCHECK(!built_);
  widget_init_params_.activatable =
      activatable ? views::Widget::InitParams::Activatable::kYes
                  : views::Widget::InitParams::Activatable::kNo;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetShowState(
    ui::mojom::WindowShowState show_state) {
  DCHECK(!built_);
  widget_init_params_.show_state = show_state;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetWindowId(int window_id) {
  DCHECK(!built_);
  window_id_ = window_id;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetWindowTitle(
    const std::u16string& title) {
  DCHECK(!built_);
  window_title_ = title;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetShow(bool show) {
  DCHECK(!built_);
  show_ = show;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetTestWidgetDelegate() {
  widget_init_params_.delegate = new TestWidgetDelegate();
  return *this;
}

std::unique_ptr<views::Widget> TestWidgetBuilder::BuildOwnsNativeWidget() {
  return BuildWidgetWithOwnership(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
}

std::unique_ptr<views::Widget> TestWidgetBuilder::BuildClientOwnsWidget() {
  return BuildWidgetWithOwnership(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
}

std::unique_ptr<views::Widget> TestWidgetBuilder::BuildWidgetWithOwnership(
    views::Widget::InitParams::Ownership ownership) {
  DCHECK(!built_);
  built_ = true;

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget_init_params_.ownership = ownership;
  widget->Init(std::move(widget_init_params_));
  if (window_id_ != aura::Window::kInitialId)
    widget->GetNativeWindow()->SetId(window_id_);
  if (!window_title_.empty()) {
    widget->GetNativeWindow()->SetTitle(window_title_);
  }
  if (show_)
    widget->Show();
  return widget;
}

views::Widget* TestWidgetBuilder::BuildOwnedByNativeWidget() {
  DCHECK(!built_);
  built_ = true;

  views::Widget* widget = new views::Widget();
  widget_init_params_.ownership =
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  widget->Init(std::move(widget_init_params_));
  if (window_id_ != aura::Window::kInitialId)
    widget->GetNativeWindow()->SetId(window_id_);
  if (!window_title_.empty()) {
    widget->GetNativeWindow()->SetTitle(window_title_);
  }
  if (show_)
    widget->Show();
  return widget;
}

}  // namespace ash

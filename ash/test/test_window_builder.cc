// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_window_builder.h"

#include "ash/shell.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

TestWindowBuilder::TestWindowBuilder() = default;

TestWindowBuilder::TestWindowBuilder(TestWindowBuilder& others)
    : parent_(others.parent_),
      context_(others.context_),
      delegate_(others.delegate_),
      window_type_(others.window_type_),
      layer_type_(others.layer_type_),
      bounds_(others.bounds_),
      init_properties_(std::move(others.init_properties_)),
      window_id_(others.window_id_),
      window_title_(others.window_title_),
      show_(others.show_) {
  DCHECK(!others.built_);
  others.built_ = true;
}

TestWindowBuilder::~TestWindowBuilder() = default;

TestWindowBuilder& TestWindowBuilder::SetParent(aura::Window* parent) {
  DCHECK(!built_);
  DCHECK(!parent_);
  parent_ = parent;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowType(
    aura::client::WindowType type) {
  DCHECK(!built_);
  window_type_ = type;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowId(int id) {
  DCHECK(!built_);
  window_id_ = id;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowTitle(
    const std::u16string& title) {
  DCHECK(!built_);
  window_title_ = title;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  bounds_ = bounds;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetDelegate(
    aura::WindowDelegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetColorWindowDelegate(SkColor color) {
  DCHECK(!built_);
  DCHECK(!delegate_);
  delegate_ = new aura::test::ColorTestWindowDelegate(color);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetTestWindowDelegate() {
  DCHECK(!built_);
  DCHECK(!delegate_);
  delegate_ = aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  return *this;
}

TestWindowBuilder& TestWindowBuilder::AllowAllWindowStates() {
  DCHECK(!built_);
  init_properties_.SetProperty(aura::client::kResizeBehaviorKey,
                               aura::client::kResizeBehaviorCanFullscreen |
                                   aura::client::kResizeBehaviorCanMaximize |
                                   aura::client::kResizeBehaviorCanMinimize |
                                   aura::client::kResizeBehaviorCanResize);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetShow(bool show) {
  DCHECK(!built_);
  show_ = show;
  return *this;
}

std::unique_ptr<aura::Window> TestWindowBuilder::Build() {
  DCHECK(!built_);
  built_ = true;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(delegate_, window_type_);
  window->Init(layer_type_);
  window->AcquireAllPropertiesFrom(std::move(init_properties_));
  if (window_id_ != aura::Window::kInitialId)
    window->SetId(window_id_);
  if (!window_title_.empty()) {
    window->SetTitle(window_title_);
  }
  if (parent_) {
    if (!bounds_.IsEmpty())
      window->SetBounds(bounds_);
    parent_->AddChild(window.get());
  } else {
    // Resolve context to find a parent.
    if (bounds_.IsEmpty()) {
      context_ = Shell::GetPrimaryRootWindow();
    } else {
      display::Display display =
          display::Screen::GetScreen()->GetDisplayMatching(bounds_);
      aura::Window* root = Shell::GetRootWindowForDisplayId(display.id());
      gfx::Point origin = bounds_.origin();
      ::wm::ConvertPointFromScreen(root, &origin);
      context_ = root;
      if (!bounds_.IsEmpty())
        window->SetBounds(gfx::Rect(origin, bounds_.size()));
    }

    DCHECK(context_);
    aura::client::ParentWindowWithContext(window.get(), context_, bounds_,
                                          display::kInvalidDisplayId);
  }
  if (show_)
    window->Show();
  return window;
}

TestWindowBuilder ChildTestWindowBuilder(aura::Window* parent,
                                         const gfx::Rect& bounds,
                                         int window_id) {
  return TestWindowBuilder().SetParent(parent).SetBounds(bounds).SetWindowId(
      window_id);
}

}  // namespace ash

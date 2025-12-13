// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_window_builder.h"

#include "ash/shell.h"
#include "ash/utility/client_controlled_state_util.h"
#include "base/functional/callback_helpers.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
using SignalCallback =
    base::RepeatingCallback<void(TestWindowBuilder::Operation)>;

namespace {

ClientControlledStateUtil::StateChangeRequestCallback CreateStateChangeCallback(
    SignalCallback signal_callback) {
  return base::BindRepeating(
      [](SignalCallback signal_callback, WindowState* window_state,
         ClientControlledState* state, chromeos::WindowStateType next_state) {
        ClientControlledStateUtil::ApplyWindowStateRequest(window_state, state,
                                                           next_state);
        if (!signal_callback.is_null()) {
          signal_callback.Run(TestWindowBuilder::kStateChange);
        }
      },
      signal_callback);
}

ClientControlledStateUtil::BoundsChangeRequestCallback
CreateBoundsChangeCallback(SignalCallback signal_callback) {
  return base::BindRepeating(
      [](SignalCallback signal_callback, WindowState* window_state,
         ClientControlledState* state,
         chromeos::WindowStateType requested_state,
         const gfx::Rect& bounds_in_display, int64_t display_id) {
        ClientControlledStateUtil::ApplyBoundsRequest(
            window_state, state, requested_state, bounds_in_display,
            display_id);
        if (!signal_callback.is_null()) {
          signal_callback.Run(TestWindowBuilder::kBoundsChange);
        }
      },
      signal_callback);
}

}  // namespace

TestWindowBuilder::TestWindowBuilder(aura::test::WindowBuilderParams params)
    : aura::test::TestWindowBuilder(std::move(params)) {}

TestWindowBuilder::TestWindowBuilder(TestWindowBuilder& others)
    : aura::test::TestWindowBuilder(others),
      operation_signal_callback_(std::move(others.operation_signal_callback_)) {
}

TestWindowBuilder::~TestWindowBuilder() = default;

TestWindowBuilder& TestWindowBuilder::SetParent(aura::Window* parent) {
  aura::test::TestWindowBuilder::SetParent(parent);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowType(
    aura::client::WindowType type) {
  aura::test::TestWindowBuilder::SetWindowType(type);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowId(int id) {
  aura::test::TestWindowBuilder::SetWindowId(id);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetBounds(const gfx::Rect& bounds) {
  aura::test::TestWindowBuilder::SetBounds(bounds);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowTitle(
    const std::u16string& title) {
  aura::test::TestWindowBuilder::SetWindowTitle(title);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetDelegate(
    aura::WindowDelegate* delegate) {
  aura::test::TestWindowBuilder::SetDelegate(delegate);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetColorWindowDelegate(SkColor color) {
  aura::test::TestWindowBuilder::SetColorWindowDelegate(color);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetTestWindowDelegate() {
  aura::test::TestWindowBuilder::SetTestWindowDelegate();
  return *this;
}

TestWindowBuilder& TestWindowBuilder::AllowAllWindowStates() {
  aura::test::TestWindowBuilder::AllowAllWindowStates();
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetShow(bool show) {
  aura::test::TestWindowBuilder::SetShow(show);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetClientControlled(
    SignalCallback signal_callback) {
  DCHECK(!built());
  operation_signal_callback_ = signal_callback;
  return *this;
}

std::unique_ptr<aura::Window> TestWindowBuilder::Build() {
  auto window = CreateWindowInternal();
  auto* parent = release_parent();

  if (parent) {
    if (!params().bounds.IsEmpty()) {
      window->SetBounds(params().bounds);
    }
    parent->AddChild(window.get());
  } else {
    aura::Window* context = nullptr;
    // Resolve context to find a parent.
    if (params().bounds.IsEmpty()) {
      context = Shell::GetPrimaryRootWindow();
    } else {
      display::Display display =
          display::Screen::Get()->GetDisplayMatching(params().bounds);
      aura::Window* root = Shell::GetRootWindowForDisplayId(display.id());
      gfx::Point origin = params().bounds.origin();
      ::wm::ConvertPointFromScreen(root, &origin);
      context = root;
      window->SetBounds(gfx::Rect(origin, params().bounds.size()));
    }

    DCHECK(context);
    aura::client::ParentWindowWithContext(
        window.get(), context, params().bounds, display::kInvalidDisplayId);
  }
  if (operation_signal_callback_) {
    ClientControlledStateUtil::BuildAndSet(
        window.get(), CreateStateChangeCallback(*operation_signal_callback_),
        CreateBoundsChangeCallback(*operation_signal_callback_));
  }
  if (params().show) {
    window->Show();
  }
  return window;
}

TestWindowBuilder ChildTestWindowBuilder(aura::Window* parent,
                                         const gfx::Rect& bounds,
                                         int window_id) {
  return TestWindowBuilder(
      {.parent = parent, .bounds = bounds, .window_id = window_id});
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WINDOW_BUILDER_H_
#define ASH_TEST_TEST_WINDOW_BUILDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// A builder to create a aura::Window for testing purpose. Use this when you
// simply need need a window without a meaningful content (except for a color)
// or capability to drag a window or drag to resize a window. If you need these
// properties in your test, use TestWidgetBuilder instead.
//
// The builder object can be used only once to create a single window because
// ownership of some parameters has to be transferred to a created window.
class ASH_EXPORT TestWindowBuilder {
 public:
  TestWindowBuilder();
  TestWindowBuilder(TestWindowBuilder& other);
  TestWindowBuilder& operator=(TestWindowBuilder& params) = delete;
  ~TestWindowBuilder();

  // Sets parameters that are used when creating a test window.
  TestWindowBuilder& SetParent(aura::Window* parent);
  TestWindowBuilder& SetWindowType(aura::client::WindowType type);
  TestWindowBuilder& SetWindowId(int id);
  TestWindowBuilder& SetBounds(const gfx::Rect& bounds);

  // Having a non-empty title helps avoid accessibility paint check failures
  // in tests. For instance, `WindowMiniView` gets its accessible name from
  // the window title.
  TestWindowBuilder& SetWindowTitle(const std::u16string& title);

  // Set a WindowDelegate used by a test window.
  TestWindowBuilder& SetDelegate(aura::WindowDelegate* delegate);

  // Use this to create a window whose content is painted with |color|.
  // This uses aura::test::ColorTestWindowDelegate as a WindowDelegate.
  TestWindowBuilder& SetColorWindowDelegate(SkColor color);

  // Use aura::test::TestWindowDelegate as a WindowDelegate.
  TestWindowBuilder& SetTestWindowDelegate();

  // Allows the window to be resizable, maximizable and minimizable.
  TestWindowBuilder& AllowAllWindowStates();

  // A window is shown when created by default. Use this if you want not
  // to show when created.
  TestWindowBuilder& SetShow(bool show);

  // Sets the window property to be set on a test window.
  template <typename T>
  TestWindowBuilder& SetWindowProperty(const ui::ClassProperty<T>* property,
                                       T value) {
    init_properties_.SetProperty(property, value);
    return *this;
  }

  // Build a window based on the parameter already set. This can be called only
  // once and the object cannot be used to create multiple windows.
  [[nodiscard]] std::unique_ptr<aura::Window> Build();

 private:
  raw_ptr<aura::Window> parent_ = nullptr;
  raw_ptr<aura::Window> context_ = nullptr;
  raw_ptr<aura::WindowDelegate, DanglingUntriaged> delegate_ = nullptr;
  aura::client::WindowType window_type_ = aura::client::WINDOW_TYPE_NORMAL;
  ui::LayerType layer_type_ = ui::LAYER_TEXTURED;
  gfx::Rect bounds_;
  ui::PropertyHandler init_properties_;
  int window_id_ = aura::Window::kInitialId;
  std::u16string window_title_ = std::u16string();
  bool show_ = true;
  bool built_ = false;
};

// A utility function to create a window builder for child windows.
TestWindowBuilder ChildTestWindowBuilder(aura::Window* parent,
                                         const gfx::Rect& bounds = gfx::Rect(),
                                         int window_id = -1);

}  // namespace ash

#endif  // ASH_TEST_TEST_WINDOW_BUILDER_H_

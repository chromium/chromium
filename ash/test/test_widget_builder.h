// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WIDGET_BUILDER_H_
#define ASH_TEST_TEST_WIDGET_BUILDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

// A builder to create a views::Widget for testing purpose. There are three
// ways to create a widget: `BuildOwnedByNativeWidget()`,
// `BuildOwnsNativeWidget()`, and `BuildClientOwnsWidget()`.  Please refer
// to the documentation of each methods to find out which is better for your
// test cases as there are subtle differences.
class ASH_EXPORT TestWidgetBuilder {
 public:
  TestWidgetBuilder();
  TestWidgetBuilder(const TestWidgetBuilder& other) = delete;
  TestWidgetBuilder& operator=(const TestWidgetBuilder& other) = delete;
  ~TestWidgetBuilder();

  // Sets the property of views::Widget::InitParams to be used when creating
  // a widget.
  TestWidgetBuilder& SetWidgetType(views::Widget::InitParams::Type type);
  TestWidgetBuilder& SetZOrderLevel(ui::ZOrderLevel z_order);
  TestWidgetBuilder& SetBounds(const gfx::Rect& bounds);
  TestWidgetBuilder& SetParent(aura::Window* parent);
  TestWidgetBuilder& SetContext(aura::Window* context);
  TestWidgetBuilder& SetActivatable(bool activatable);
  TestWidgetBuilder& SetShowState(ui::mojom::WindowShowState show_state);

  // Sets the window property to be set on the window of a widget.
  template <typename T>
  TestWidgetBuilder& SetWindowProperty(const ui::ClassProperty<T>* property,
                                       T value) {
    widget_init_params_.init_properties_container.SetProperty(property, value);
    return *this;
  }

  // Set the window id used on the window of a test widget.
  TestWidgetBuilder& SetWindowId(int window_id);

  // Having a non-empty title helps avoid accessibility paint check failures
  // in tests. For instance, `WindowMiniView` gets its accessible name from
  // the window title.
  TestWidgetBuilder& SetWindowTitle(const std::u16string& title);

  // A widget is shown when created by default. Use this if you want not
  // to show when created.
  TestWidgetBuilder& SetShow(bool show);

  // Set the widget's delegate. It is not owned by the widget.
  TestWidgetBuilder& SetDelegate(views::WidgetDelegate* delegate);

  // Creates a test widget delegate that
  // 1) makes the window resizable, maximizable and minimizale.
  // 2) creates an ash's window frame.
  TestWidgetBuilder& SetTestWidgetDelegate();

  // Creates a widget owned by a native window (aura::Window on ChromeOS) and
  // returns a raw pointer.  Use this if you want to create a widget that
  // behaves like an application.  You should not delete the widget directly but
  // must call 'Widget::CloseWithReason' (recommended) or 'Widget::Close'. Note
  // that this is just an request, and the widget may not be closed and deleted
  // if the widget implementation rejected the request.  This is also
  // asynchronus, and instances of the widget and its window will be deleted in
  // a posted task.  There is a 'Widget::CloseNow' which forcibly and
  // synchronously closes and delete the widget and its window, but this should
  // not be used in normal situation.
  views::Widget* BuildOwnedByNativeWidget();

  // Creates a widget that owns a native window (aura::Window on ChromeOS) and
  // returns an unique pointer of the widget which owns a native window
  // (aura::Window on ChromeOS). It will be closed and deleted immediately when
  // the object exits its scope.  The important difference is that a widget
  // won't be deleted when the window is deleted first and
  // Widget::GetNativeWindow() may return nullptr. Prefer
  // BuildClientOwnsWidget() to this.
  [[nodiscard]] std::unique_ptr<views::Widget> BuildOwnsNativeWidget();

  // Creates a widget that can live independently of the native widget,
  // and where the native widget can live independently of the widget. The
  // native widget will be owned by the native window. When
  // the widget is closed, it requests that the native widget also be
  // closed and deleted, but that is allowed to happen asynchronously.
  [[nodiscard]] std::unique_ptr<views::Widget> BuildClientOwnsWidget();

 private:
  // Both BuildOwnsNativeWidget() and BuildClientOwnsWidget() are just
  // wrappers around this.
  [[nodiscard]] std::unique_ptr<views::Widget> BuildWidgetWithOwnership(
      views::Widget::InitParams::Ownership ownership);

  views::Widget::InitParams widget_init_params_{
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET};
  int window_id_ = aura::Window::kInitialId;
  std::u16string window_title_ = std::u16string();
  bool show_ = true;
  bool built_ = false;
};

}  // namespace ash

#endif  // ASH_TEST_TEST_WIDGET_BUILDER_H_

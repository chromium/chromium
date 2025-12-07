// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WINDOW_BUILDER_H_
#define ASH_TEST_TEST_WINDOW_BUILDER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/aura/test/test_window_builder.h"

namespace ash {

class ASH_EXPORT TestWindowBuilder : public aura::test::TestWindowBuilder {
 public:
  explicit TestWindowBuilder(aura::test::WindowBuilderParams = {});
  TestWindowBuilder(TestWindowBuilder& other);
  TestWindowBuilder& operator=(TestWindowBuilder& params) = delete;
  ~TestWindowBuilder();

  // aura::test::TestWindowBuilder:
  TestWindowBuilder& SetParent(aura::Window* parent);
  TestWindowBuilder& SetWindowType(aura::client::WindowType type);
  TestWindowBuilder& SetWindowId(int id);
  TestWindowBuilder& SetBounds(const gfx::Rect& bounds);
  TestWindowBuilder& SetWindowTitle(const std::u16string& title);
  TestWindowBuilder& SetDelegate(aura::WindowDelegate* delegate);
  TestWindowBuilder& SetColorWindowDelegate(SkColor color);
  TestWindowBuilder& SetTestWindowDelegate();
  TestWindowBuilder& AllowAllWindowStates();
  TestWindowBuilder& SetShow(bool show);
  template <typename T>
  TestWindowBuilder& SetWindowProperty(const ui::ClassProperty<T>* property,
                                       T value) {
    aura::test::TestWindowBuilder::SetWindowProperty(property, value);
    return *this;
  }

  enum Operation { kStateChange, kBoundsChange };

  // Creates a client controlled state backed window, which executes the
  // operation asynchronosly in a separate task.  `signal_callback` will be
  // called every time the operation's task is executed.
  TestWindowBuilder& SetClientControlled(
      base::RepeatingCallback<void(Operation)> signal_callback);

  // Build a window based on the parameter already set. This can be called only
  // once and the object cannot be used to create multiple windows.
  [[nodiscard]] std::unique_ptr<aura::Window> Build() override;

 private:
  std::optional<base::RepeatingCallback<void(Operation)>>
      operation_signal_callback_;
};

// A utility function to create a window builder for child windows.
TestWindowBuilder ChildTestWindowBuilder(aura::Window* parent,
                                         const gfx::Rect& bounds = gfx::Rect(),
                                         int window_id = -1);

}  // namespace ash

#endif  // ASH_TEST_TEST_WINDOW_BUILDER_H_

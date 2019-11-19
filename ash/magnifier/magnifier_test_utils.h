// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_
#define ASH_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class InputMethod;
}  // namespace ui

namespace ash {

class TestFocusView;
class TestTextInputView;

// Defines a test helper for magnifiers unit tests that wants to verify their
// behaviors in response to focus change events.
class MagnifierFocusTestHelper {
 public:
  MagnifierFocusTestHelper() = default;
  ~MagnifierFocusTestHelper() = default;

  static constexpr int kButtonHeight = 20;
  static constexpr gfx::Size kTestFocusViewSize{300, 200};

  // Creates a view at |location| in the primary root window with size =
  // |kTestFocusViewSize|. The view has two buttons, the first is positioned at
  // the top of the view and the second at the bottom of the view. Both bottons
  // have width = the width of |kTestFocusViewSize|, and height =
  // |kButtonHeight|.
  void CreateAndShowFocusTestView(const gfx::Point& location);

  void FocusFirstButton();
  void FocusSecondButton();

  gfx::Rect GetFirstButtonBoundsInRoot() const;
  gfx::Rect GetSecondButtonBoundsInRoot() const;

 private:
  TestFocusView* focus_test_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MagnifierFocusTestHelper);
};

// Defines a test helper for magnifiers unit tests that wants to verify their
// behaviors in response to text fields input and focus events.
class MagnifierTextInputTestHelper {
 public:
  MagnifierTextInputTestHelper() = default;
  ~MagnifierTextInputTestHelper() = default;

  // Creates a text input view in the primary root window with the given
  // |bounds|.
  void CreateAndShowTextInputView(const gfx::Rect& bounds);

  // Similar to the above, but creates the view in the given |root| window.
  void CreateAndShowTextInputViewInRoot(const gfx::Rect& bounds,
                                        aura::Window* root);

  // Returns the text input view's bounds in root window coordinates.
  gfx::Rect GetTextInputViewBounds();

  // Returns the caret bounds in root window coordinates.
  gfx::Rect GetCaretBounds();

  void FocusOnTextInputView();

  // Maximizes the widget of |text_input_view_|.
  void MaximizeWidget();

 private:
  ui::InputMethod* GetInputMethod();

  TestTextInputView* text_input_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MagnifierTextInputTestHelper);
};

}  // namespace ash

#endif  // ASH_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class TestTextInputView;

// Defines a test helper for magnifiers unit tests that wants to verify their
// behaviors in response to focus change events.
class MagnifierFocusTestHelper {
 public:
  MagnifierFocusTestHelper() = default;
  MagnifierFocusTestHelper(const MagnifierFocusTestHelper&) = delete;
  MagnifierFocusTestHelper& operator=(const MagnifierFocusTestHelper&) = delete;
  ~MagnifierFocusTestHelper() = default;

  static constexpr int kButtonHeight = 20;
  static constexpr gfx::Size kTestFocusViewSize{300, 200};
};

// Defines a test helper for magnifiers unit tests that wants to verify their
// behaviors in response to text fields input and focus events.
class MagnifierTextInputTestHelper {
 public:
  MagnifierTextInputTestHelper() = default;
  MagnifierTextInputTestHelper(const MagnifierTextInputTestHelper&) = delete;
  MagnifierTextInputTestHelper& operator=(const MagnifierTextInputTestHelper&) =
      delete;
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

  raw_ptr<TestTextInputView> text_input_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_TEST_UTILS_H_

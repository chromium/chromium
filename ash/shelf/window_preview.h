// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_WINDOW_PREVIEW_H_
#define ASH_SHELF_WINDOW_PREVIEW_H_

#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace ash {

class WindowPreviewView;

// A view used by the shelf which shows a mirror view of the the window
// associated with the window of the shelf icon where the mouse is hovered over.
// The view is also contains a button which closes the window if clicked. Other
// click events will activate the window and dismiss the bubble which holds this
// view.
class WindowPreview : public views::View {
 public:
  class Delegate {
   public:
    // Returns the maximum ratio across all current preview windows.
    virtual float GetMaxPreviewRatio() const = 0;

    // Notify the delegate that the preview has closed.
    virtual void OnPreviewDismissed(WindowPreview* preview) = 0;

    // Notify the delegate that the preview has been activated.
    virtual void OnPreviewActivated(WindowPreview* preview) = 0;

   protected:
    virtual ~Delegate() {}
  };

  WindowPreview(aura::Window* window, Delegate* delegate);

  WindowPreview(const WindowPreview&) = delete;
  WindowPreview& operator=(const WindowPreview&) = delete;

  ~WindowPreview() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  const WindowPreviewView* preview_view() const { return preview_view_; }

 private:
  // All the preview containers have the same size.
  gfx::Size GetPreviewContainerSize() const;

  void CloseButtonPressed();

  // Child views.
  views::ImageButton* close_button_ = nullptr;
  views::Label* title_ = nullptr;
  views::View* preview_container_view_ = nullptr;
  WindowPreviewView* preview_view_ = nullptr;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  Delegate* delegate_;
};

}  // namespace ash

#endif  // ASH_SHELF_WINDOW_PREVIEW_H_

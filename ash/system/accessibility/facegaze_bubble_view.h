// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_

#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ui {
class MouseEvent;
}  // namespace ui

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class FaceGazeBubbleCloseView;
class FaceGazeBubbleMainContentView;

// The FaceGaze bubble view. This is a UI that appears at the top of the screen,
// which tells the user the most recently recognized facial gesture and the
// corresponding action that was taken. It also exposes a close button so that
// the feature can be conveniently turned off, if necessary. This view is only
// visible when the FaceGaze feature is enabled.
class ASH_EXPORT FaceGazeBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(FaceGazeBubbleView, views::BubbleDialogDelegateView)

 public:
  explicit FaceGazeBubbleView(
      const base::RepeatingCallback<void()>& on_mouse_entered,
      const base::RepeatingCallback<void(const ui::Event& event)>&
          on_close_button_clicked);
  FaceGazeBubbleView(const FaceGazeBubbleView&) = delete;
  FaceGazeBubbleView& operator=(const FaceGazeBubbleView&) = delete;
  ~FaceGazeBubbleView() override;

  // Updates text content of this view.
  void Update(const std::u16string& text, bool is_warning);

  std::u16string_view GetTextForTesting() const;

  const raw_ptr<FaceGazeBubbleCloseView> GetCloseViewForTesting() const {
    return close_view_;
  }

 private:
  friend class FaceGazeBubbleControllerTest;
  friend class FaceGazeBubbleTestHelper;

  // Updates color of this view.
  void UpdateColor(bool is_warning);

  // The view containing the main content, such as the FaceGaze icon and the
  // informational text. Owned by the views hierarchy.
  raw_ptr<FaceGazeBubbleMainContentView> main_content_view_ = nullptr;

  // The view containing the close button, which can be used to quickly turn
  // FaceGaze off. Owned by the views hierarchy.
  raw_ptr<FaceGazeBubbleCloseView> close_view_ = nullptr;
};

// The main content view. This is the part of the bubble UI that tells the user
// the most recently recognized facial gesture and the corresponding action that
// was taken.
class ASH_EXPORT FaceGazeBubbleMainContentView : public views::View {
  METADATA_HEADER(FaceGazeBubbleMainContentView, views::View)

 public:
  FaceGazeBubbleMainContentView(
      const base::RepeatingCallback<void()>& on_mouse_entered);
  FaceGazeBubbleMainContentView(const FaceGazeBubbleMainContentView&) = delete;
  FaceGazeBubbleMainContentView& operator=(
      const FaceGazeBubbleMainContentView&) = delete;
  ~FaceGazeBubbleMainContentView() override;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;

  // Updates the text content, visibility, and color of this view.
  void Update(const std::u16string& text, bool is_warning);

  views::Label* label() { return label_; }

 private:
  // Updates color of this view.
  void UpdateColor(bool is_warning);

  // Custom callback that is called whenever the mouse enters or exits this
  // view.
  const base::RepeatingCallback<void()> on_mouse_entered_;

  // An image that displays the FaceGaze logo.
  raw_ptr<views::ImageView> image_ = nullptr;

  // A label that displays the most recently recognized gesture and
  // corresponding action.
  raw_ptr<views::Label> label_ = nullptr;
};

// The close button view. It contains the button used to quickly toggle
// FaceGaze off.
class ASH_EXPORT FaceGazeBubbleCloseView : public views::View {
  METADATA_HEADER(FaceGazeBubbleCloseView, views::View)

 public:
  FaceGazeBubbleCloseView(
      const base::RepeatingCallback<void(const ui::Event& event)>&
          on_close_button_clicked);
  FaceGazeBubbleCloseView(const FaceGazeBubbleCloseView&) = delete;
  FaceGazeBubbleCloseView& operator=(const FaceGazeBubbleCloseView&) = delete;
  ~FaceGazeBubbleCloseView() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  // Custom callback that is called whenever the close button is clicked.
  const base::RepeatingCallback<void(const ui::Event& event)>
      on_close_button_clicked_;

  // The close 'X' image.
  raw_ptr<views::ImageView> close_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_

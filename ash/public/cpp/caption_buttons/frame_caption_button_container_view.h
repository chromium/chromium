// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include <map>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/caption_buttons/frame_size_button_delegate.h"
#include "ash/public/cpp/caption_buttons/snap_controller.h"
#include "base/macros.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/window/frame_caption_button.h"

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace views {
class Widget;
}

namespace ash {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
class ASH_PUBLIC_EXPORT FrameCaptionButtonContainerView
    : public views::View,
      public views::ButtonListener,
      public FrameSizeButtonDelegate,
      public views::AnimationDelegateViews {
 public:
  static const char kViewClassName[];

  // |frame| is the views::Widget that the caption buttons act on.
  explicit FrameCaptionButtonContainerView(views::Widget* frame);
  ~FrameCaptionButtonContainerView() override;

  // For testing.
  class ASH_PUBLIC_EXPORT TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {}

    void EndAnimations();

    views::FrameCaptionButton* minimize_button() const {
      return container_view_->minimize_button_;
    }

    views::FrameCaptionButton* size_button() const {
      return container_view_->size_button_;
    }

    views::FrameCaptionButton* close_button() const {
      return container_view_->close_button_;
    }

    views::FrameCaptionButton* menu_button() const {
      return container_view_->menu_button_;
    }

   private:
    FrameCaptionButtonContainerView* container_view_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Sets the id of the vector image to paint the button for |icon|. The
  // FrameCaptionButtonContainerView will keep track of the image to use for
  // |icon| even if none of the buttons currently use |icon|.
  void SetButtonImage(views::CaptionButtonIcon icon,
                      const gfx::VectorIcon& icon_definition);

  // Sets whether the buttons should be painted as active. Does not schedule
  // a repaint.
  void SetPaintAsActive(bool paint_as_active);

  // Sets the background frame color that buttons should compute their color
  // respective to.
  void SetBackgroundColor(SkColor background_color);

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Updates the caption buttons' state based on the caption button model's
  // state. A parent view should relayout to reflect the change in states.
  void UpdateCaptionButtonState(bool animate);

  // Sets the size of the buttons in this container.
  void SetButtonSize(const gfx::Size& size);

  // Sets the CaptionButtonModel. Caller is responsible for updating
  // the state by calling UpdateCaptionButtonState.
  void SetModel(std::unique_ptr<CaptionButtonModel> model);
  const CaptionButtonModel* model() const { return model_.get(); }

  // views::View:
  void Layout() override;
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  friend class FrameCaptionButtonContainerViewTest;

  // Sets |button|'s icon to |icon|. If |animate| is ANIMATE_YES, the button
  // will crossfade to the new icon. If |animate| is ANIMATE_NO and
  // |icon| == |button|->icon(), the crossfade animation is progressed to the
  // end.
  void SetButtonIcon(views::FrameCaptionButton* button,
                     views::CaptionButtonIcon icon,
                     Animate animate);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // FrameSizeButtonDelegate:
  bool IsMinimizeButtonVisible() const override;
  void SetButtonsToNormal(Animate animate) override;
  void SetButtonIcons(views::CaptionButtonIcon minimize_button_icon,
                      views::CaptionButtonIcon close_button_icon,
                      Animate animate) override;
  const views::FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const override;
  void SetHoveredAndPressedButtons(
      const views::FrameCaptionButton* to_hover,
      const views::FrameCaptionButton* to_press) override;
  bool CanSnap() override;
  void ShowSnapPreview(SnapDirection snap) override;
  void CommitSnap(SnapDirection snap) override;

  // The widget that the buttons act on.
  views::Widget* frame_;

  // The buttons. In the normal button style, at most one of |minimize_button_|
  // and |size_button_| is visible.
  views::FrameCaptionButton* menu_button_ = nullptr;
  views::FrameCaptionButton* minimize_button_ = nullptr;
  views::FrameCaptionButton* size_button_ = nullptr;
  views::FrameCaptionButton* close_button_ = nullptr;

  // Mapping of the image needed to paint a button for each of the values of
  // CaptionButtonIcon.
  std::map<views::CaptionButtonIcon, const gfx::VectorIcon*> button_icon_map_;

  // Animation that affects the visibility of |size_button_| and the position of
  // buttons to the left of it. Usually this is just the minimize button but it
  // can also include a PWA menu button.
  std::unique_ptr<gfx::SlideAnimation> tablet_mode_animation_;

  std::unique_ptr<CaptionButtonModel> model_;

  DISALLOW_COPY_AND_ASSIGN(FrameCaptionButtonContainerView);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

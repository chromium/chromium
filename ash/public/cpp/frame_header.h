// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FRAME_HEADER_H_
#define ASH_PUBLIC_CPP_FRAME_HEADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

enum class AshLayoutSize;

// Helper class for managing the window header.
class ASH_PUBLIC_EXPORT FrameHeader : public gfx::AnimationDelegate {
 public:
  enum Mode { MODE_ACTIVE, MODE_INACTIVE };

  ~FrameHeader() override;

  // Returns the header's minimum width.
  int GetMinimumHeaderWidth() const;

  // Paints the header.
  void PaintHeader(gfx::Canvas* canvas, Mode mode);

  // Performs layout for the header.
  void LayoutHeader();

  // Get the height of the header.
  int GetHeaderHeight() const;

  // Gets / sets how much of the header is painted. This allows the header to
  // paint under things (like the tabstrip) which have transparent /
  // non-painting sections. This height does not affect LayoutHeader().
  int GetHeaderHeightForPainting() const;
  void SetHeaderHeightForPainting(int height_for_painting);

  // Schedule a re-paint of the entire title.
  void SchedulePaintForTitle();

  // True to instruct the frame header to paint the header as an active
  // state.
  void SetPaintAsActive(bool paint_as_active);

  // Called when frame show state is changed.
  void OnShowStateChanged(ui::WindowShowState show_state);

  void SetLeftHeaderView(views::View* view);
  void SetBackButton(FrameCaptionButton* view);
  FrameCaptionButton* GetBackButton() const;

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // Sets text to display in place of the window's title. This will be shown
  // regardless of what WidgetDelegate::ShouldShowWindowTitle() returns.
  void SetFrameTextOverride(const base::string16& frame_text_override);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  void set_button_color_mode(FrameCaptionButton::ColorMode button_color_mode) {
    button_color_mode_ = button_color_mode;
  }

 protected:
  FrameHeader(views::Widget* target_widget, views::View* view);

  views::Widget* target_widget() { return target_widget_; }
  const views::Widget* target_widget() const { return target_widget_; }

  // Returns bounds of the region in |view_| which is painted with the header
  // images. The region is assumed to start at the top left corner of |view_|
  // and to have the same width as |view_|.
  gfx::Rect GetPaintedBounds() const;

  void UpdateCaptionButtonColors();

  void PaintTitleBar(gfx::Canvas* canvas);

  void SetCaptionButtonContainer(
      FrameCaptionButtonContainerView* caption_button_container);

  views::View* view() { return view_; }

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  Mode mode() const { return mode_; }

  const gfx::SlideAnimation& activation_animation() {
    return activation_animation_;
  }

  virtual void DoPaintHeader(gfx::Canvas* canvas) = 0;
  // Updates the frame colors. The parameters may or may not be ignored.
  // TODO(estade): remove these parameters and instead always set them via Aura
  // window properties, as is done with CustomFrameHeader.
  virtual void DoSetFrameColors(SkColor active_frame_color,
                                SkColor inactive_frame_color) = 0;
  virtual AshLayoutSize GetButtonLayoutSize() const = 0;
  virtual SkColor GetTitleColor() const = 0;
  virtual SkColor GetCurrentFrameColor() const = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, BackButtonAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, TitleIconAlignment);
  FRIEND_TEST_ALL_PREFIXES(DefaultFrameHeaderTest, FrameColors);

  void LayoutHeaderInternal();

  gfx::Rect GetTitleBounds() const;

  FrameCaptionButton::ColorMode button_color_mode_ =
      FrameCaptionButton::ColorMode::kDefault;

  // The widget that the caption buttons act on. This can be different from
  // |view_|'s widget.
  views::Widget* target_widget_;

  // The view into which |this| paints.
  views::View* view_;
  FrameCaptionButton* back_button_ = nullptr;  // May remain nullptr.
  views::View* left_header_view_ = nullptr;    // May remain nullptr.
  FrameCaptionButtonContainerView* caption_button_container_ = nullptr;

  // The height of the header to paint.
  int painted_height_ = 0;

  // Whether the header should be painted as active.
  Mode mode_ = MODE_INACTIVE;

  // Whether the header is painted for the first time.
  bool initial_paint_ = true;

  base::string16 frame_text_override_;

  gfx::SlideAnimation activation_animation_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameHeader);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FRAME_HEADER_H_

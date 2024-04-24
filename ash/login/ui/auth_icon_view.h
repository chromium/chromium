// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_ICON_VIEW_H_
#define ASH_LOGIN_UI_AUTH_ICON_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// An icon with a built-in progress bar functionality and animation support used
// to show auth factors (e.g. Fingerprint, Smart Lock) in the
// LoginAuthFactorsView.
class ASH_EXPORT AuthIconView : public views::View {
  METADATA_HEADER(AuthIconView, views::View)

 public:
  enum class Status {
    kPrimary,
    kDisabled,
    kError,
    kPositive,
  };

  static ui::ColorId GetColorId(AuthIconView::Status status);

  AuthIconView();
  AuthIconView(AuthIconView&) = delete;
  AuthIconView& operator=(AuthIconView&) = delete;
  ~AuthIconView() override;

  // Show a static icon.
  void SetIcon(const gfx::VectorIcon& icon, Status status = Status::kPrimary);
  // Rasterize the icon.
  void RasterizeIcon();

  // Show a circle icon.
  void SetCircleImage(int size, SkColor color);

  // Show a sequence of animation frames. |animation_resource_id| should refer
  // to an image with the frames of the animation layed out horizontally.
  // |duration| is the total duration of the animation. |num_frames| is the
  // number of frames in the image referred to by |animation_resource_id|.
  void SetAnimation(int animation_resource_id,
                    base::TimeDelta duration,
                    int num_frames);

  // Play a Lottie animation. The animation will play exactly once, after which
  // the final frame will be displayed until the icon is changed again.
  void SetLottieAnimation(std::unique_ptr<lottie::Animation> animation);

  // Cause the icon to briefly shake left and right to signify that an error has
  // occurred.
  void RunErrorShakeAnimation();

  // Cause the icon to repeatedly emit a circle that gradually scales up and
  // fades out in order to nudge user to click.
  void RunNudgeAnimation();

  // Starts a progress spinner animation if not already running.
  void StartProgressAnimation();

  // Stops the progress spinner animation if running.
  void StopProgressAnimation();

  // Stops any existing animations.
  void StopAnimating();

  void set_on_tap_or_click_callback(base::RepeatingClosure on_tap_or_click) {
    on_tap_or_click_callback_ = on_tap_or_click;
  }

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  // Helper class to draw a circle that can be converted to "gfx::ImageSkia"
  class CircleImageSource : public gfx::CanvasImageSource {
   public:
    explicit CircleImageSource(int size, SkColor color);
    CircleImageSource(const CircleImageSource&) = delete;
    CircleImageSource& operator=(const CircleImageSource&) = delete;
    ~CircleImageSource() override = default;

    void Draw(gfx::Canvas* canvas) override;

   private:
    SkColor color_;
  };

  base::RepeatingClosure on_tap_or_click_callback_;

  raw_ptr<AnimatedRoundedImageView> icon_;
  raw_ptr<views::AnimatedImageView> lottie_animation_view_;
  ui::ImageModel icon_image_model_;

  // Time when the progress animation was enabled.
  base::TimeTicks progress_animation_start_time_;

  // Used to schedule paint calls for the progress animation.
  base::RepeatingTimer progress_animation_timer_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_ICON_VIEW_H_

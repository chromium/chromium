// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_

#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/lottie/animation_observer.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class PillButton;

// Contains congratulatory text for the user completing a focus session, as well
// as buttons to complete the session or add 10 minutes. This is only shown when
// the session duration is reached. Also owns the `animation_widget_` that will
// play an animation on top of the emoji.
class FocusModeEndingMomentView : public views::FlexLayoutView,
                                  public lottie::AnimationObserver {
  METADATA_HEADER(FocusModeEndingMomentView, views::FlexLayoutView)

 public:
  FocusModeEndingMomentView();
  FocusModeEndingMomentView(const FocusModeEndingMomentView&) = delete;
  FocusModeEndingMomentView& operator=(const FocusModeEndingMomentView&) =
      delete;
  ~FocusModeEndingMomentView() override;

  // lottie::AnimationObserver:
  void AnimationCycleEnded(const lottie::Animation* animation) override;

  // Used to set if `extend_session_duration_button_` should be enabled or not
  // and to show the ending moment bubble contents and play the emoji animation.
  void ShowEndingMomentContents(bool extend_button_enabled);

 private:
  friend class FocusModeTrayTest;

  // Creates the animation widget if one doesn't already exist.
  void CreateAnimationWidget();

  // The `+10 min` button.
  raw_ptr<PillButton> extend_session_duration_button_ = nullptr;

  // The emoji label adjacent to the congratulatory text.
  raw_ptr<views::Label> emoji_label_ = nullptr;

  // The animation widget that plays on top of `emoji_label_`.
  std::unique_ptr<views::Widget> animation_widget_;

  base::ScopedObservation<lottie::Animation, lottie::AnimationObserver>
      scoped_animation_observer_{this};

  base::WeakPtrFactory<FocusModeEndingMomentView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_

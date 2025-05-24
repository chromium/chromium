// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_FINGERPRINT_VIEW_H_
#define ASH_AUTH_VIEWS_FINGERPRINT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/style/ash_color_id.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/lottie/animation_observer.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// FingerprintView is a view displaying a fingerprint icon and label,
// dynamically adapting based on fingerprint availability,
// authentication state, and the presence of a PIN.
class ASH_EXPORT FingerprintView : public views::View,
                                   public lottie::AnimationObserver {
  METADATA_HEADER(FingerprintView, views::View)
 public:
  class TestApi {
   public:
    explicit TestApi(FingerprintView* view);
    ~TestApi();

    void SetEnabled(bool enabled);
    bool GetEnabled() const;

    FingerprintView* GetView();

    views::Label* GetLabel();

    AnimatedRoundedImageView* GetIcon();

    FingerprintState GetState() const;

    void ShowFirstFrame();
    void ShowLastFrame();

   private:
    const raw_ptr<FingerprintView> view_;
  };

  FingerprintView();

  FingerprintView(const FingerprintView&) = delete;
  FingerprintView& operator=(const FingerprintView&) = delete;

  ~FingerprintView() override;

  // views::View:
  // Informs the user to use the fingerprint sensor upon touch/tap.
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the view's appearance based on the given fingerprint state.
  void SetState(FingerprintState state);

  // Indicates if a PIN is set, potentially influencing the label text.
  void SetHasPin(bool has_pin);

  // Triggers a brief animation to signal an authentication success.
  void NotifyAuthSuccess(base::OnceClosure on_success_animation_finished);

  // Triggers a brief animation to signal an authentication failure.
  void NotifyAuthFailure();

  // views::View:
  // Calculates the preferred size of the view.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // lottie::AnimationObserver:
  void AnimationCycleEnded(const lottie::Animation* animation) override;

 private:
  // Updates the visual elements to reflect the current state and PIN
  // availability.
  void DisplayCurrentState();

  // Helper functions to configure the icon and label based on the current
  // state.
  void SetIcon();
  ui::ColorId GetIconColorIdFromState() const;
  int GetTextIdFromState() const;
  int GetA11yTextIdFromState() const;
  bool NeedA11yAlertFromState() const;

  // Visual components:
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<AnimatedRoundedImageView> icon_ = nullptr;

  // A green checkmark animation shown when NotifyAuthSuccess called.
  raw_ptr<views::AnimatedImageView> lottie_animation_view_;

  base::ScopedObservation<lottie::Animation, lottie::AnimationObserver>
      scoped_animation_observer_{this};

  base::OnceClosure on_success_animation_finished_;

  bool has_success_ = false;

  // State:
  FingerprintState state_ = FingerprintState::UNAVAILABLE;
  bool has_pin_ = false;

  // Timer for transition  between states.
  base::OneShotTimer reset_state_;

  base::WeakPtrFactory<FingerprintView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_FINGERPRINT_VIEW_H_

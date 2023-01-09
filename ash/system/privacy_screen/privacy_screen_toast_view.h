// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

class FeaturePodIconButton;
class PrivacyScreenToastLabelView;
class PrivacyScreenToastController;

// The view shown inside the privacy screen toast bubble.
class ASH_EXPORT PrivacyScreenToastView : public views::View,
                                          public views::ViewObserver {
 public:
  PrivacyScreenToastView(PrivacyScreenToastController* controller,
                         views::Button::PressedCallback callback);
  ~PrivacyScreenToastView() override;
  PrivacyScreenToastView(PrivacyScreenToastView&) = delete;
  PrivacyScreenToastView operator=(PrivacyScreenToastView&) = delete;

  // Updates the toast with whether the privacy screen is enabled and managed.
  void SetPrivacyScreenEnabled(bool enabled, bool managed);

  // Returns true if the toggle button is focused.
  bool IsButtonFocused() const;

 private:
  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  PrivacyScreenToastController* controller_ = nullptr;
  FeaturePodIconButton* button_ = nullptr;
  PrivacyScreenToastLabelView* label_ = nullptr;
  bool is_enabled_ = false;
  bool is_managed_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_TOAST_VIEW_H_

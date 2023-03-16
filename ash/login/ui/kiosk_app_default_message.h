// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_KIOSK_APP_DEFAULT_MESSAGE_H_
#define ASH_LOGIN_UI_KIOSK_APP_DEFAULT_MESSAGE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "base/scoped_observation.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// The implementation of kiosk app default message for the shelf.
// KioskAppDefaultMessage is owned by itself and would be destroyed when its
// widget got destroyed, which happened when the widget's window got destroyed.
class ASH_EXPORT KioskAppDefaultMessage
    : public LoginBaseBubbleView,
      public ShelfBackgroundAnimatorObserver {
 public:
  KioskAppDefaultMessage();

  KioskAppDefaultMessage(const KioskAppDefaultMessage&) = delete;
  KioskAppDefaultMessage& operator=(const KioskAppDefaultMessage&) = delete;
  ~KioskAppDefaultMessage() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // LoginBaseBubbleView
  gfx::Point CalculatePosition() override;

 private:
  views::ImageView* icon_ = nullptr;
  views::Label* title_ = nullptr;

  ShelfBackgroundAnimator background_animator_;
  base::ScopedObservation<ShelfBackgroundAnimator,
                          ShelfBackgroundAnimatorObserver>
      background_animator_observation_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_KIOSK_APP_DEFAULT_MESSAGE_H_

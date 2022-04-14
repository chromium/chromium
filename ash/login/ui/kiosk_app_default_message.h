// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_KIOSK_APP_DEFAULT_MESSAGE_H_
#define ASH_LOGIN_UI_KIOSK_APP_DEFAULT_MESSAGE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_background_animator.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "base/scoped_observation.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
namespace views {
class BubbleDialogDelegateView;
class View;
}  // namespace views

namespace ash {

// The implementation of kiosk app default message for the shelf.
class ASH_EXPORT KioskAppDefaultMessage
    : public views::BubbleDialogDelegateView,
      public ShelfBackgroundAnimatorObserver {
 public:
  KioskAppDefaultMessage();
  KioskAppDefaultMessage(const KioskAppDefaultMessage&) = delete;
  KioskAppDefaultMessage& operator=(const KioskAppDefaultMessage&) = delete;
  ~KioskAppDefaultMessage() override;

  // views::View:
  void OnThemeChanged() override;

  void Show();
  void Hide();

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
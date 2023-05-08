// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_
#define ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class ASH_EXPORT BootingAnimationController {
 public:
  BootingAnimationController();
  BootingAnimationController(const BootingAnimationController&) = delete;
  BootingAnimationController& operator=(const BootingAnimationController&) =
      delete;
  ~BootingAnimationController();

  // Shows the widget and starts to play a booting animation.
  void Show();

  // Cleans up the animation, resets the widget and the view.
  void Finish();

 private:
  void OnAnimationDataFetched(std::string data);
  void StartAnimation();

  std::string animation_data_;
  views::UniqueWidgetPtr widget_;
  bool start_once_ready_ = false;

  base::WeakPtrFactory<BootingAnimationController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_

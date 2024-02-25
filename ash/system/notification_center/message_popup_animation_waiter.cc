// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_popup_animation_waiter.h"

#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "base/functional/bind.h"

namespace ash {

MessagePopupAnimationWaiter::MessagePopupAnimationWaiter(
    AshMessagePopupCollection* message_popup_collection)
    : message_popup_collection_(message_popup_collection) {
  DCHECK(message_popup_collection_);
}

MessagePopupAnimationWaiter::~MessagePopupAnimationWaiter() = default;

void MessagePopupAnimationWaiter::Wait() {
  if (message_popup_collection_->popups_animating_for_test()) {
    message_popup_collection_->SetAnimationIdleClosureForTest(  // IN-TEST
        base::BindOnce(&MessagePopupAnimationWaiter::OnPopupAnimationFinished,
                       weak_ptr_factory_.GetWeakPtr()));
    run_loop_.Run();
  }
}

void MessagePopupAnimationWaiter::OnPopupAnimationFinished() {
  run_loop_.Quit();
}

}  // namespace ash

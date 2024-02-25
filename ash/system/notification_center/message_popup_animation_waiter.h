// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_POPUP_ANIMATION_WAITER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_POPUP_ANIMATION_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"

namespace ash {
class AshMessagePopupCollection;

// A test helper that waits for all message popup animations to finish.
class MessagePopupAnimationWaiter {
 public:
  explicit MessagePopupAnimationWaiter(
      AshMessagePopupCollection* message_popup_collection);
  MessagePopupAnimationWaiter(const MessagePopupAnimationWaiter&) = delete;
  MessagePopupAnimationWaiter& operator=(const MessagePopupAnimationWaiter&) =
      delete;
  ~MessagePopupAnimationWaiter();

  // Waits for the popup animations managed by `message_popup_collection_` to
  // complete. No op if `message_popup_collection_` is already idle.
  void Wait();

 private:
  // Called when all popup animations finish.
  void OnPopupAnimationFinished();

  // The message popup collection whose animations are being waited for.
  const raw_ptr<AshMessagePopupCollection> message_popup_collection_;

  base::RunLoop run_loop_;

  base::WeakPtrFactory<MessagePopupAnimationWaiter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_POPUP_ANIMATION_WAITER_H_

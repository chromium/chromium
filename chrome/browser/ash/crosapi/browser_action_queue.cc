// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_action_queue.h"

#include "base/check.h"
#include "chrome/browser/ash/crosapi/browser_action.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"

namespace crosapi {

BrowserActionQueue::BrowserActionQueue() = default;
BrowserActionQueue::~BrowserActionQueue() = default;

bool BrowserActionQueue::IsEmpty() const {
  return actions_.empty();
}

void BrowserActionQueue::PushOrCancel(std::unique_ptr<BrowserAction> action,
                                      mojom::CreationResult cancel_reason) {
  if (action->IsQueueable()) {
    actions_.push(std::move(action));
  } else {
    action->Cancel(cancel_reason);
  }
}

void BrowserActionQueue::Push(std::unique_ptr<BrowserAction> action) {
  DCHECK(action->IsQueueable());
  actions_.push(std::move(action));
}

std::unique_ptr<BrowserAction> BrowserActionQueue::Pop() {
  DCHECK(!IsEmpty());
  std::unique_ptr<BrowserAction> action = std::move(actions_.front());
  actions_.pop();
  return action;
}

void BrowserActionQueue::Clear() {
  base::queue<std::unique_ptr<BrowserAction>> empty;
  actions_.swap(empty);
  DCHECK(IsEmpty());
}

}  // namespace crosapi

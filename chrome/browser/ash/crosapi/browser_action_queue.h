// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_QUEUE_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_QUEUE_H_

#include <memory>

#include "base/containers/queue.h"

namespace crosapi {

namespace mojom {
enum class CreationResult;
}  // namespace mojom

class BrowserAction;

// A queue of queueable actions.
class BrowserActionQueue {
 public:
  BrowserActionQueue();
  ~BrowserActionQueue();
  // Enqueues |action| if it is queueable. Cancels it otherwise.
  void PushOrCancel(std::unique_ptr<BrowserAction> action,
                    mojom::CreationResult cancel_reason);
  void Push(std::unique_ptr<BrowserAction> action);
  std::unique_ptr<BrowserAction> Pop();
  bool IsEmpty() const;
  void Clear();

 private:
  base::queue<std::unique_ptr<BrowserAction>> actions_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_QUEUE_H_

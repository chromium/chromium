// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/browser_process_task_provider.h"

#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace task_manager {

BrowserProcessTaskProvider::BrowserProcessTaskProvider() {
}

BrowserProcessTaskProvider::~BrowserProcessTaskProvider() {
}

Task* BrowserProcessTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                      int route_id) {
  if (child_id == network::mojom::kBrowserProcessId)
    return &browser_process_task_;

  return nullptr;
}

void BrowserProcessTaskProvider::StartUpdating() {
  NotifyObserverTaskAdded(&browser_process_task_);
}

void BrowserProcessTaskProvider::StopUpdating() {
  // There's nothing to do here. The browser process task live as long as the
  // browser lives and when StopUpdating() is called the |observer_| has already
  // been cleared.
}

}  // namespace task_manager

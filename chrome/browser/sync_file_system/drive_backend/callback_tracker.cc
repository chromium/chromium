// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/callback_tracker.h"

#include <algorithm>

namespace sync_file_system {
namespace drive_backend {

CallbackTracker::CallbackTracker() {
}

CallbackTracker::~CallbackTracker() {
  AbortAll();
}

void CallbackTracker::AbortAll() {
  AbortClosureByHelper helpers;
  std::swap(helpers, helpers_);
  for (auto itr = helpers.begin(); itr != helpers.end(); ++itr) {
    delete itr->first;
    std::move(itr->second).Run();
  }
}

std::unique_ptr<internal::AbortHelper> CallbackTracker::PassAbortHelper(
    internal::AbortHelper* helper) {
  if (helpers_.erase(helper) == 1)
    return std::unique_ptr<internal::AbortHelper>(helper);
  return nullptr;
}

}  // namespace drive_backend
}  // namespace sync_file_system

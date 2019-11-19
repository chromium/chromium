// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/callback_tracker_internal.h"

#include "chrome/browser/sync_file_system/drive_backend/callback_tracker.h"

namespace sync_file_system {
namespace drive_backend {
namespace internal {

AbortHelper::AbortHelper(CallbackTracker* tracker) : tracker_(tracker) {}

AbortHelper::~AbortHelper() {}

base::WeakPtr<AbortHelper> AbortHelper::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
std::unique_ptr<AbortHelper> AbortHelper::TakeOwnership(
    const base::WeakPtr<AbortHelper>& abort_helper) {
  if (!abort_helper)
    return nullptr;
  std::unique_ptr<AbortHelper> result =
      abort_helper->tracker_->PassAbortHelper(abort_helper.get());
  abort_helper->weak_ptr_factory_.InvalidateWeakPtrs();
  return result;
}

}  // namespace internal
}  // namespace drive_backend
}  // namespace sync_file_system

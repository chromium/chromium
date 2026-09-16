// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"

#include <utility>

#include "base/memory/scoped_refptr.h"

OneTimePermissionsConditionTracker::Factory::Factory(
    base::RepeatingCallback<void(const url::Origin&)>
        on_all_references_released)
    : on_all_references_released_(std::move(on_all_references_released)) {}

OneTimePermissionsConditionTracker::Factory::~Factory() = default;

scoped_refptr<OneTimePermissionsConditionTracker>
OneTimePermissionsConditionTracker::Factory::New(const url::Origin& origin) {
  auto it = map_.find(origin);
  if (it != map_.end()) {
    return base::WrapRefCounted(it->second);
  }
  auto new_entry =
      base::MakeRefCounted<OneTimePermissionsConditionTracker>(base::BindOnce(
          &Factory::OnTrackerDestroyed, weak_factory_.GetWeakPtr(), origin));
  map_[origin] = new_entry.get();
  return new_entry;
}

void OneTimePermissionsConditionTracker::Factory::OnTrackerDestroyed(
    const url::Origin& origin) {
  // Erase the map entry first to ensure we don't have a dangling pointer.
  map_.erase(origin);

  if (on_all_references_released_) {
    on_all_references_released_.Run(origin);
  }
}

OneTimePermissionsConditionTracker::OneTimePermissionsConditionTracker(
    base::OnceClosure on_destruction)
    : on_destruction_(std::move(on_destruction)) {}

OneTimePermissionsConditionTracker::~OneTimePermissionsConditionTracker() {
  if (on_destruction_) {
    std::move(on_destruction_).Run();
  }
}

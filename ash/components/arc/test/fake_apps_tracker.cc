// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_apps_tracker.h"

namespace arc {
namespace data_snapshotd {

FakeAppsTracker::FakeAppsTracker() = default;
FakeAppsTracker::~FakeAppsTracker() = default;

void FakeAppsTracker::StartTracking(
    base::RepeatingCallback<void(int)> update_callback,
    base::OnceClosure finish_callback) {
  start_tracking_num_++;
  update_callback_ = std::move(update_callback);
  finish_callback_ = std::move(finish_callback);
}

}  // namespace data_snapshotd
}  // namespace arc

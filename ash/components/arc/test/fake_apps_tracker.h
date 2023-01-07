// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_

#include "ash/components/arc/enterprise/arc_apps_tracker.h"
#include "base/callback.h"

namespace arc {
namespace data_snapshotd {

// Fake implementation of ArcAppsTracker for tests.
class FakeAppsTracker : public ArcAppsTracker {
 public:
  FakeAppsTracker();
  FakeAppsTracker(const FakeAppsTracker&) = delete;
  FakeAppsTracker& operator=(const FakeAppsTracker&) = delete;
  ~FakeAppsTracker() override;

  // ArcAppsTracker overrides:
  void StartTracking(base::RepeatingCallback<void(int)> update_callback,
                     base::OnceClosure finish_callback) override;

  base::RepeatingCallback<void(int)>& update_callback() {
    return update_callback_;
  }
  base::OnceClosure finish_callback() { return std::move(finish_callback_); }
  int start_tracking_num() const { return start_tracking_num_; }

 private:
  int start_tracking_num_ = 0;
  base::RepeatingCallback<void(int)> update_callback_;
  base::OnceClosure finish_callback_;
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_APPS_TRACKER_H_

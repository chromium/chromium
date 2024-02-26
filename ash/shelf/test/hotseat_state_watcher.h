// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_TEST_HOTSEAT_STATE_WATCHER_H_
#define ASH_SHELF_TEST_HOTSEAT_STATE_WATCHER_H_

#include "ash/shelf/shelf_layout_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Records HotseatState transitions.
class HotseatStateWatcher : public ShelfLayoutManagerObserver {
 public:
  explicit HotseatStateWatcher(ShelfLayoutManager* shelf_layout_manager);
  ~HotseatStateWatcher() override;

  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  void CheckEqual(std::vector<HotseatState> state_changes);

  void WaitUntilStateChanged();

 private:
  raw_ptr<ShelfLayoutManager> shelf_layout_manager_;
  std::vector<HotseatState> state_changes_;
  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // ASH_SHELF_TEST_HOTSEAT_STATE_WATCHER_H_

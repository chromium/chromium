// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class BirchBarView;
class BirchItem;

// The controller used to manage the birch bar in every `OverviewGrid`. It will
// fetch data from `BirchModel` and distribute the data to birch bars.
class BirchBarController {
 public:
  BirchBarController();
  BirchBarController(const BirchBarController&) = delete;
  BirchBarController& operator=(const BirchBarController&) = delete;
  ~BirchBarController();

  // Gets the instance of the controller. It can be nullptr when the Overview
  // session is shutting down.
  static BirchBarController* Get();

  // Register a bar view with its initialized callback which will be called
  // after initialization.
  void RegisterBar(BirchBarView* bar_view,
                   base::OnceClosure bar_initialized_callback);

  // Called if the given `bar_view` is being destroyed.
  void OnBarDestroying(BirchBarView* bar_view);

 private:
  // Called when birch items are fetched from model or the fetching process
  // timed out.
  void OnItemsFecthedFromModel();

  // initialize the given `bar_view` with the items fetched from model.
  void InitBar(BirchBarView* bar_view);

  // Birch items fetched from model.
  std::vector<std::unique_ptr<BirchItem>> items_;

  // The map of each bar view to corresponding initialized callback.
  base::flat_map<raw_ptr<BirchBarView>, base::OnceClosure> bar_map_;

  // Indicates if the data fetching process completes or not.
  bool data_fetch_complete_ = false;

  base::WeakPtrFactory<BirchBarController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTROLLER_H_

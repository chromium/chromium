// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_PUBLIC_EXPORT AppListControllerObserver
    : public base::CheckedObserver {
 public:
  // Called when the AppList target visibility changes.
  virtual void OnAppListVisibilityWillChange(bool shown, int64_t display_id) {}

  // Called when the AppList visibility changes.
  virtual void OnAppListVisibilityChanged(bool shown, int64_t display_id) {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_OBSERVER_H_

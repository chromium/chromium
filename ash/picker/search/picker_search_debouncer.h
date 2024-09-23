// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_DEBOUNCER_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_DEBOUNCER_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

class ASH_EXPORT PickerSearchDebouncer {
 public:
  explicit PickerSearchDebouncer(base::TimeDelta delay);

  // Request to call `search` if there are no other calls to `RequestSearch`
  // within the delay specified in the constructor.
  void RequestSearch(base::OnceClosure search);

  bool IsSearchPending();

 private:
  base::TimeDelta delay_;
  base::OneShotTimer timer_;
};

}  // namespace ash

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_DEBOUNCER_H_

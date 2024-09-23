// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_debouncer.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

PickerSearchDebouncer::PickerSearchDebouncer(base::TimeDelta delay)
    : delay_(std::move(delay)) {}

void PickerSearchDebouncer::RequestSearch(base::OnceClosure search) {
  timer_.Start(FROM_HERE, delay_, std::move(search));
}

bool PickerSearchDebouncer::IsSearchPending() {
  return timer_.IsRunning();
}

}  // namespace ash

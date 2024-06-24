// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/test_watchdog.h"

namespace ash::cfm {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

TestWatchDog::TestWatchDog(mojo::PendingReceiver<mojom::DataWatchDog> receiver,
                           mojom::DataFilterPtr filter)
    : receiver_(this, std::move(receiver)), filter_(std::move(filter)) {}

TestWatchDog::~TestWatchDog() {}  // IN-TEST

const mojom::DataFilterPtr TestWatchDog::GetFilter() {
  return filter_->Clone();
}

void TestWatchDog::OnNotify(const std::string& data) {
  std::string display_name;
  if (filter_->filter_type == CHANGE) {
    display_name = "[CHANGE]";
  } else {
    display_name = "[" + filter_->pattern.value_or("NO_PATTERN") + "]";
  }

  VLOG(4) << display_name << " watchdog tripped!";
}

}  // namespace ash::cfm

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit.h"

namespace resource_coordinator {

LifecycleUnit::SortKey::SortKey() = default;

LifecycleUnit::SortKey::SortKey(base::TimeTicks last_focused_time)
    : last_focused_time(last_focused_time) {}


LifecycleUnit::SortKey::SortKey(const SortKey& other) = default;

bool LifecycleUnit::SortKey::operator<(const SortKey& other) const {
  return last_focused_time < other.last_focused_time;
}

bool LifecycleUnit::SortKey::operator>(const SortKey& other) const {
  return other < *this;
}

LifecycleUnit::~LifecycleUnit() = default;

}  // namespace resource_coordinator

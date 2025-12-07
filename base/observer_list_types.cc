// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_types.h"

namespace base {

CheckedObserver::CheckedObserver() = default;
CheckedObserver::~CheckedObserver() = default;

bool CheckedObserver::IsInObserverList() const {
  return factory_.HasWeakPtrs();
}

}  // namespace base

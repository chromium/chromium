// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"

#include "components/performance_manager/performance_manager_impl.h"

namespace resource_coordinator {

ResourceCoordinatorParts::ResourceCoordinatorParts()
#if !defined(OS_ANDROID)
    : tab_manager_(&tab_load_tracker_),
      tab_lifecycle_unit_source_(tab_manager_.usage_clock())
#endif
{
#if !defined(OS_ANDROID)
  tab_lifecycle_unit_source_.AddObserver(&tab_manager_);
#endif
}

ResourceCoordinatorParts::~ResourceCoordinatorParts() = default;

}  // namespace resource_coordinator

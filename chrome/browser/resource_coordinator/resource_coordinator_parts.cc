// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"

#include "build/build_config.h"

namespace resource_coordinator {

ResourceCoordinatorParts::ResourceCoordinatorParts()
#if !BUILDFLAG(IS_ANDROID)
    : tab_lifecycle_unit_source_(tab_manager_.usage_clock())
#endif
{
#if !BUILDFLAG(IS_ANDROID)
  tab_lifecycle_unit_source_.AddObserver(&tab_manager_);
#endif
}

ResourceCoordinatorParts::~ResourceCoordinatorParts() = default;

}  // namespace resource_coordinator

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace features {

BASE_DECLARE_FEATURE(kCustomizedTabLoadTimeout);
BASE_DECLARE_FEATURE(kTabRanker);

}  // namespace features

namespace resource_coordinator {

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

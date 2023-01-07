// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::user_tuning {

bool IsRefreshRateThrottled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return base::FeatureList::IsEnabled(
             performance_manager::features::kBatterySaverModeAvailable) &&
         UserPerformanceTuningManager::GetInstance()->IsBatterySaverActive();
#endif
}

}  //  namespace performance_manager::user_tuning

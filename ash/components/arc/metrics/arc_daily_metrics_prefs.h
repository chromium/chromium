// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_PREFS_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_PREFS_H_

class PrefRegistrySimple;

namespace arc {

// Registering prefs needs access to metrics::DailyEvent, which isn't possible
// in arc_prefs.cc
void RegisterDailyMetricsPrefs(PrefRegistrySimple* registry);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_PREFS_H_

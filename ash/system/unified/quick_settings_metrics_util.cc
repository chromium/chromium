// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_metrics_util.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// For the revamped view:
constexpr char kQuickSettingsButton[] = "Ash.QuickSettings.Button.Activated";

// For the old view:
constexpr char kUnifiedViewButton[] = "Ash.UnifiedSystemView.Button.Activated";

}  // namespace

namespace quick_settings_metrics_util {

void RecordQsButtonActivated(const QsButtonCatalogName button_catalog_name,
                             const ui::Event& event) {
  base::UmaHistogramEnumeration(
      features::IsQsRevampEnabled() ? kQuickSettingsButton : kUnifiedViewButton,
      button_catalog_name);
}

}  // namespace quick_settings_metrics_util

}  // namespace ash

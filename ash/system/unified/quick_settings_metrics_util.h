// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_

#include "ash/constants/quick_settings_catalogs.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash::quick_settings_metrics_util {

// Records any event on a button in the quick settings main page. The value of
// recording type of event (such as: tap/click/stylus etc) is not high. To avoid
// creating a bunch of metrics, this method only records the "catalog name" as
// the enum bucket for now. Leaves the `event` as a arg in the method for later
// use, so that if the event type need to be tracked later we can simply add
// them in this method.
void RecordQsButtonActivated(const QsButtonCatalogName button_catalog_name,
                             const ui::Event& event);

}  // namespace ash::quick_settings_metrics_util

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_

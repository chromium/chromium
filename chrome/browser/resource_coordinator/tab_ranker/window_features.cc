// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/window_features.h"

namespace tab_ranker {

WindowFeatures::WindowFeatures(
    metrics::WindowMetricsEvent::Type type,
    metrics::WindowMetricsEvent::ShowState show_state,
    bool is_active,
    int tab_count)
    : type(type),
      show_state(show_state),
      is_active(is_active),
      tab_count(tab_count) {}

bool WindowFeatures::operator==(const WindowFeatures& other) const {
  return type == other.type && show_state == other.show_state &&
         is_active == other.is_active && tab_count == other.tab_count;
}

bool WindowFeatures::operator!=(const WindowFeatures& other) const {
  return !operator==(other);
}

}  // namespace tab_ranker

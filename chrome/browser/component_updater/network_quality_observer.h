// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_NETWORK_QUALITY_OBSERVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_NETWORK_QUALITY_OBSERVER_H_

#include "services/network/public/cpp/network_quality_tracker.h"

namespace component_updater {

// Creates and registers a NetworkQualityObserver with `network_quality_tracker`
// if one does not exist already.
void EnsureNetworkQualityObserver();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_NETWORK_QUALITY_OBSERVER_H_

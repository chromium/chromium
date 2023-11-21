// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/prefs/pref_service.h"

namespace data_controls {

// Sets the Data Controls policy for testing.
void SetDataControls(PrefService* prefs, std::vector<std::string> rules);

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_UTILS_H_

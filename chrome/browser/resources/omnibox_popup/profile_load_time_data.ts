// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds the loadTimeData values that depend on the profile of the window that
// shows the popup. In TopChromeWebUIProfile, the WebUIDataSource of the popup
// is shared by the windows of all profiles, so OmniboxPopupUI passes these
// values to each frame instead. In other profiles, nothing is passed. Entry
// points must import this before the elements that read the values.
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';

const profileLoadTimeData = chrome.getVariableValue('profileLoadTimeData');
if (profileLoadTimeData) {
  loadTimeData.overrideValues(JSON.parse(profileLoadTimeData));
}

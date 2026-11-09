// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds the loadTimeData values that depend on the profile of the window that
// shows the organizer panel. In TopChromeWebUIProfile, the WebUIDataSource of
// the panel is shared by the windows of all profiles, so OrganizerPanelUI
// passes these values to each frame instead. In other profiles, nothing is
// passed, and chrome.getVariableValue() isn't available. Entry points must
// import '/strings.m.js' and then this module before the elements that read
// the values.
import {loadTimeData} from '//resources/js/load_time_data.js';

const profileLoadTimeData = chrome.getVariableValue?.('profileLoadTimeData');
if (profileLoadTimeData) {
  loadTimeData.overrideValues(JSON.parse(profileLoadTimeData));
}

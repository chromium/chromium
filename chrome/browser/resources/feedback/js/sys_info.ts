// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {createLogsMapTable} from './logs_map_page.js';

function getSystemInformation():
    Promise<chrome.feedbackPrivate.LogsMapEntry[]> {
  return new Promise(
      resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
}


// Configures color change listener for dynamic colors. Used when sys_info
// dialog is opened in OS Feedback.
// TODO(b/276493287): Move add class to HTML when jelly colors and OS Feedback
// launches by default in ASH.
function configureJellyColors() {
  if (loadTimeData.getBoolean('isJellyEnabledForOsFeedback')) {
    document.body.classList.add('jelly-enabled');
    startColorChangeUpdater();
  }
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  getSystemInformation().then(createLogsMapTable);
  configureJellyColors();
};

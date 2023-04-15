// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Main entry point for the set time dialog
 */

import './strings.m.js';
import './set_time_dialog.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

// TODO(b/257329722) After the Jelly experiment is launched, add the CSS link
// element directly to the HTML.
const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
if (jellyEnabled) {
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://theme/colors.css?sets=legacy,sys';
  document.head.appendChild(link);
  document.body.classList.add('jelly-enabled');
}

window.onload = () => {
  if (jellyEnabled) {
    startColorChangeUpdater();
  }
};

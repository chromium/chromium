// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

function initialize() {
  // TODO(https://crbug.com/454597786): Setup logic for the native OS
  // integration (Mojo, ResizeObserver, etc.) will be injected here in the
  // subsequent CL in this chain.
  ColorChangeUpdater.forDocument().start();

  // Hide the second step if the user cannot pin to taskbar.
  if (!loadTimeData.getBoolean('canPinToTaskbar')) {
    const step2 = document.getElementById('step-2');
    if (step2) {
      step2.hidden = true;
    }
  }
}

document.addEventListener('DOMContentLoaded', initialize);

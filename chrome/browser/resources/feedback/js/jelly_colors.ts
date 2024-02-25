// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../../strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  // Configures color change listener for dynamic colors. Used when sys_info
  // dialog is opened in OS Feedback.
  // TODO(b/276493287): Move add class to HTML when jelly colors and OS Feedback
  // launches by default in ASH.
  document.body.classList.add('jelly-enabled');
  ColorChangeUpdater.forDocument().start();
};

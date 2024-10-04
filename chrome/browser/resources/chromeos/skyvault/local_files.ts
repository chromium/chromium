// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Main entry point for the local files migration dialog
 */

import './strings.m.js';
import './local_files_migration_dialog.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

window.addEventListener('load', () => {
  const updater = ColorChangeUpdater.forDocument();
  updater.start();
  updater.refreshColorsCss();
});

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */

import './strings.m.js';
import './lock_screen_reauth.js';

import {$} from 'chrome://resources/ash/common/util.js';

export {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

function initialize() {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in chrome://resources/ash/common/util.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  (window as any).$ = $;
}

initialize();

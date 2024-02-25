// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './password_change.js';

import {$} from 'chrome://resources/ash/common/util.js';

function initialize() {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in chrome://resources/ash/common/util.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  window.$ = $;
}

initialize();
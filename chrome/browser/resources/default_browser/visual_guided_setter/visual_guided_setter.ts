// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

function initialize() {
  // TODO(https://crbug.com/454597786): Setup logic for the native OS
  // integration (Mojo, ResizeObserver, etc.) will be injected here in the
  // subsequent CL in this chain.
  ColorChangeUpdater.forDocument().start();
}

document.addEventListener('DOMContentLoaded', initialize);

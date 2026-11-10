/* Copyright 2025 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import './history_sync_optin_app.js';

import {browserProxyFactory} from './history_sync_optin.mojom-webui.js';

function initialize() {
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // even when the page zoom in Chrome is different than 100%.
  browserProxyFactory.getInstance().handler.updateDialogHeight(
      document.body.offsetHeight);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
}

document.addEventListener('DOMContentLoaded', initialize);

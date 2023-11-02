/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';

function initialize() {
  const syncConfirmationBrowserProxy =
      SyncConfirmationBrowserProxyImpl.getInstance();
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // even when the page zoom in Chrome is different than 100%.
  syncConfirmationBrowserProxy.initializedWithSize(
      [document.body.offsetHeight]);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
}

document.addEventListener('DOMContentLoaded', initialize);

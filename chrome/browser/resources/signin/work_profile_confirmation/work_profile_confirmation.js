/* Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {WorkProfileConfirmationBrowserProxyImpl} from './work_profile_confirmation_browser_proxy.js';

document.addEventListener('DOMContentLoaded', () => {
  const workProfileConfirmationBrowserProxy =
      WorkProfileConfirmationBrowserProxyImpl.getInstance();
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // even when the page zoom in Chrome is different than 100%.
  workProfileConfirmationBrowserProxy.initializedWithSize(
      [document.body.offsetHeight]);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
});

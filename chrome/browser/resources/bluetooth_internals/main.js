// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {checkSystemPermissions, initializeViews} from './bluetooth_internals.js';
import {BluetoothInternalsHandler} from './bluetooth_internals.mojom-webui.js';

document.addEventListener('DOMContentLoaded', async () => {
  // Using a query of "isTest" provides a hook for the test suite to perform
  // setup actions before initializing the UI.
  const params = new URLSearchParams(window.location.search);
  const isTest = params.has('isTest');
  if (!isTest) {
    checkSystemPermissions(
        BluetoothInternalsHandler.getRemote(), initializeViews);
  }
});

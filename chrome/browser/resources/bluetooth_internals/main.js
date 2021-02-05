// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {initializeViews} from './bluetooth_internals.js';

document.addEventListener('DOMContentLoaded', () => {
  // Using a query of "test" provides a hook for the test suite to perform
  // setup actions.
  const params = new URLSearchParams(window.location.search);
  const test = params.get('test');
  if (test) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = `chrome://test/bluetooth_internals/${test}`;
    document.body.appendChild(script);
  } else {
    initializeViews();
  }
});

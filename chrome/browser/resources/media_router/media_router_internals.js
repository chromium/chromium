// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

// Handles user events for the Media Router Internals UI.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('getStatus').then(status => {
    const jsonStatus = JSON.stringify(status, null, /* spacing level = */ 2);
    $('sink-status-div').textContent = jsonStatus;
  });
});

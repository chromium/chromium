// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_state.html, served from chrome://local-state/
 * This is used to debug the contents of the Local State file.
 */
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

// When the page loads, request the JSON local state data from C++.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('requestJson').then(localState => {
    $('content').textContent = localState;
  });
});

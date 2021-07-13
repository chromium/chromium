// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

// TODO(crbug/1213937): Launch Projector toolbar and integrate with screen
// capture.
function onLaunchClick() {
  sendWithPromise('launchScreenCapture').then(function(isVisible) {
    var button = document.body.querySelector('button');
    // TODO(crbug/1213937): Use $i18n{}.
    if (isVisible) {
      button.textContent = 'Hide Projector Tools';
    } else {
      button.textContent = 'Show Projector Tools';
    }
  });
}

function initialize() {
  document.body.querySelector('button').onclick = onLaunchClick;
}

document.addEventListener('DOMContentLoaded', initialize, false);

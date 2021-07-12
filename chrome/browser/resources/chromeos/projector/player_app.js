// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug/1213937): Launch Projector toolbar and integrate with screen
// capture.
function onLaunchClick() {
  console.log('Launching Projector toolbar');
}

function initialize() {
  document.body.querySelector('button').onclick = onLaunchClick;
}

document.addEventListener('DOMContentLoaded', initialize, false);

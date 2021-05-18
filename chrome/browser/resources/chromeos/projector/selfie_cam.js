// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Grabs video feed from user's camera. If the user's device does not have a
 * camera, then the error is caught below.
 * @param {!MediaStreamConstraints} constraints
 */
async function getMedia(constraints) {
  let stream = null;
  try {
    stream = await navigator.mediaDevices.getUserMedia(constraints);
    document.body.querySelector('video').srcObject = stream;
  } catch (err) {
    console.error(err.name + ': ' + err.message);
  }
}

// Since the selfie cam is circular, we want equal width and height.
document.addEventListener('DOMContentLoaded', function() {
  getMedia({video: {width: window.innerWidth, height: window.innerHeight}});
}, false);

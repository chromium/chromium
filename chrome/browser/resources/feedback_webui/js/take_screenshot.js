// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Function to take the screenshot of the current screen.
 * @param {function(HTMLCanvasElement)} callback Callback for returning the
 *     canvas with the screenshot. Called with null if the screenshot failed.
 */
function takeScreenshot(callback) {
  let screenshotStream = null;
  const video = document.createElement('video');

  video.addEventListener('canplay', function(e) {
    if (screenshotStream) {
      const canvas = document.createElement('canvas');
      canvas.setAttribute('width', video.videoWidth);
      canvas.setAttribute('height', video.videoHeight);
      canvas.getContext('2d').drawImage(
          video, 0, 0, video.videoWidth, video.videoHeight);

      video.pause();
      video.srcObject = null;

      screenshotStream.getVideoTracks()[0].stop();
      screenshotStream = null;

      callback(canvas);
    }
  }, false);

  navigator.webkitGetUserMedia(
      {
        video: {
          mandatory:
              {chromeMediaSource: 'screen', maxWidth: 4096, maxHeight: 2560}
        }
      },
      function(stream) {
        if (stream) {
          screenshotStream = stream;
          video.srcObject = screenshotStream;
          video.play();
        }
      },
      function(err) {
        console.error(
            'takeScreenshot failed: ' + err.name + '; ' + err.message + '; ' +
            err.constraintName);
        callback(null);
      });
}

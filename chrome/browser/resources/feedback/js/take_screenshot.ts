// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare global {
  type GetUserMediaError = Error&{constraintName: string};

  interface Navigator {
    webkitGetUserMedia(
        params: any, callback: (stream?: MediaStream) => void,
        errorCallback: (error: GetUserMediaError) => void): void;
  }
}

/**
 * Function to take the screenshot of the current screen.
 * @param {function(?HTMLCanvasElement)} callback Callback for returning the
 *     canvas with the screenshot. Called with null if the screenshot failed.
 */
export function takeScreenshot(
    callback: (canvas: HTMLCanvasElement|null) => void) {
  let screenshotStream: MediaStream|null = null;
  const video = document.createElement('video');

  video.addEventListener('canplay', function() {
    if (screenshotStream) {
      const canvas = document.createElement('canvas');
      canvas.setAttribute('width', video.videoWidth.toString());
      canvas.setAttribute('height', video.videoHeight.toString());
      canvas.getContext('2d')!.drawImage(
          video, 0, 0, video.videoWidth, video.videoHeight);

      video.pause();
      video.srcObject = null;

      screenshotStream.getVideoTracks()[0]!.stop();
      screenshotStream = null;

      callback(canvas);
    }
  }, false);

  navigator.webkitGetUserMedia(
      {
        video: {
          mandatory:
              {chromeMediaSource: 'screen', maxWidth: 4096, maxHeight: 2560},
        },
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

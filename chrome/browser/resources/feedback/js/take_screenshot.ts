// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {FeedbackBrowserProxyImpl} from './feedback_browser_proxy.js';

/**
 * Function to take the screenshot of the current screen.
 * @return A Promise holidng the canvas with the screenshot or null if the
 *     screenshot failed.
 */
export function takeScreenshot(): Promise<HTMLCanvasElement|null> {
  let screenshotStream: MediaStream|null = null;
  const video = document.createElement('video');

  const resolver = new PromiseResolver<HTMLCanvasElement|null>();

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

      resolver.resolve(canvas);
    }
  }, false);

  FeedbackBrowserProxyImpl.getInstance()
      .getUserMedia({
        video: {
          mandatory:
              {chromeMediaSource: 'screen', maxWidth: 4096, maxHeight: 2560},
        },
      })
      .then(
          function(stream) {
            if (stream) {
              screenshotStream = stream;
              video.srcObject = screenshotStream;
              video.play();
            } else {
              // Dummy codepath to satisfy tests where no MediaStream exists.
              resolver.resolve(document.createElement('canvas'));
            }
          },
          function(err) {
            console.error(
                'takeScreenshot failed: ' + err.name + '; ' + err.message +
                '; ' + err.constraintName);
            resolver.resolve(null);
          });

  return resolver.promise;
}

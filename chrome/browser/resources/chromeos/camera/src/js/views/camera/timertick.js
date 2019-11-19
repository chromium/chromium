// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};
/**
 * Namespace for camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Namespace for timertick.
 */
cca.views.camera.timertick = cca.views.camera.timertick || {};

/**
 * Handler to cancel the active running timer-ticks.
 * @type {function()}
 * @private
 */
cca.views.camera.timertick.cancel_ = null;

/**
 * Starts timer ticking if applicable.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.timertick.start = function() {
  cca.views.camera.timertick.cancel_ = null;
  if (!cca.state.get('timer')) {
    return Promise.resolve();
  }
  return new Promise((resolve, reject) => {
    var tickTimeout = null;
    var tickMsg = document.querySelector('#timer-tick-msg');
    cca.views.camera.timertick.cancel_ = () => {
      if (tickTimeout) {
        clearTimeout(tickTimeout);
        tickTimeout = null;
      }
      cca.util.animateCancel(tickMsg);
      reject(new Error('cancel'));
    };

    let tickCounter = cca.state.get('_10sec') ? 10 : 3;
    const sounds = {
      1: '#sound-tick-final',
      2: '#sound-tick-inc',
      3: '#sound-tick-inc',
      [tickCounter]: '#sound-tick-start',
    };
    var onTimerTick = () => {
      if (tickCounter == 0) {
        resolve();
      } else {
        if (sounds[tickCounter] !== undefined) {
          cca.sound.play(sounds[tickCounter]);
        }
        tickMsg.textContent = tickCounter + '';
        cca.util.animateOnce(tickMsg);
        tickTimeout = setTimeout(onTimerTick, 1000);
        tickCounter--;
      }
    };
    // First tick immediately in the next message loop cycle.
    tickTimeout = setTimeout(onTimerTick, 0);
  });
};

/**
 * Cancels active timer ticking if applicable.
 */
cca.views.camera.timertick.cancel = function() {
  if (cca.views.camera.timertick.cancel_) {
    cca.views.camera.timertick.cancel_();
    cca.views.camera.timertick.cancel_ = null;
  }
};

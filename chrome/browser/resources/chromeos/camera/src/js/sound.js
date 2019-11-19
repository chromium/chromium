// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for sound.
 */
cca.sound = cca.sound || {};

/**
 * Plays a sound.
 * @param {string} selector Selector of the sound.
 * @return {!Promise} Promise for waiting finishing playing or canceling wait.
 */
cca.sound.play = function(selector) {
  // Use a timeout to wait for sound finishing playing instead of end-event
  // as it might not be played at all (crbug.com/135780).
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  var cancel;
  var p = new Promise((resolve, reject) => {
    var element = document.querySelector(selector);
    var timeout = setTimeout(resolve, Number(element.dataset.timeout || 0));
    cancel = () => {
      clearTimeout(timeout);
      reject(new Error('cancel'));
    };
    element.currentTime = 0;
    element.play();
  });
  p.cancel = cancel;
  return p;
};

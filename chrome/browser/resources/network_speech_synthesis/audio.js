// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Initialize the audio element and event listeners on it.
window.audioElement_ = document.createElement('audio');
document.body.appendChild(window.audioElement_);
window.audioElement_.addEventListener('canplaythrough', onStart_, false);
window.audioElement_.addEventListener('ended', onStop_, false);

/**
 * Handler for the canplaythrough event on the audio element.
 * Called when the audio element has buffered enough audio to begin
 * playback. Send the 'start' event to the ttsEngine callback and
 * then begin playing the audio element.
 * @private
 */
function onStart_() {
  if (!window.currentUtterance_) {
    return;
  }

  if (window.currentUtterance_.options.volume !== undefined) {
    // Both APIs use the same range for volume, between 0.0 and 1.0.
    window.audioElement_.volume = window.currentUtterance_.options.volume;
  }
  window.audioElement_.play();
  chrome.runtime.sendMessage({command: 'onStart'});
}

function onStop_() {
  window.currentUtterance_ = null;
  chrome.runtime.sendMessage({command: 'onStop'});
}

chrome.runtime.onMessage.addListener(message => {
  switch (message['command']) {
    case 'pause':
      window.audioElement_.pause();
      break;
    case 'play':
      window.audioElement_.play();
      break;
    case 'setCurrentUtterance':
      window.currentUtterance_ = message.currentUtterance;
      break;
    case 'setUrl':
      window.audioElement_.src = message.url;
      break;
  }
});

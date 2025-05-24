// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class AudioHandler {
  constructor() {
    // Initialize the audio element and event listeners on it.
    this.audioElement_ = document.createElement('audio');
    document.body.appendChild(this.audioElement_);
    this.audioElement_.addEventListener(
        'canplaythrough', () => this.onCanPlayThrough_(), false);
    this.audioElement_.addEventListener('ended', () => this.onEnded_(), false);

    // The extension system does not keep the main service worker awake even if
    // we're doing work in this offscreen doc. Keep it awake while we're playing
    // speech via this interval.
    this.heartbeatId_ = 0;

    chrome.runtime.onMessage.addListener(message => {
      switch (message['command']) {
        case 'pause':
          this.audioElement_.pause();
          break;
        case 'play':
          this.audioElement_.play();
          break;
        case 'setCurrentUtterance':
          this.currentUtterance_ = message.currentUtterance;
          break;
        case 'setUrl':
          this.audioElement_.src = message.url;
          break;
      }
    });
  }

  /**
   * Handler for the canplaythrough event on the audio element.
   * Called when the audio element has buffered enough audio to begin
   * playback. Send the 'start' event to the ttsEngine callback and
   * then begin playing the audio element.
   * @private
   */
  onCanPlayThrough_() {
    if (!this.currentUtterance_) {
      return;
    }

    if (this.currentUtterance_.options.volume !== undefined) {
      // Both APIs use the same range for volume, between 0.0 and 1.0.
      this.audioElement_.volume = this.currentUtterance_.options.volume;
    }
    this.audioElement_.play();
    chrome.runtime.sendMessage({command: 'onCanPlayThrough'});

    this.heartbeatId_ = setInterval(this.onHeartbeat_, 1000);
  }

  onEnded_() {
    this.currentUtterance_ = null;
    chrome.runtime.sendMessage({command: 'onEnded'});
    clearInterval(this.heartbeatId_);
  }

  onHeartbeat_() {
    chrome.runtime.sendMessage({command: 'heartbeat'});
  }
}

document.addEventListener('DOMContentLoaded', () => {
  new AudioHandler();
  chrome.runtime.sendMessage({command: 'loaded'});
});

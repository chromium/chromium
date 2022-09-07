// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function VoiceInput(keyboard) {
  this.finaResult_ = null;
  this.recognizing_ = false;
  this.keyboard_ = keyboard;
  this.recognition_ = new webkitSpeechRecognition();
  this.recognition_.onstart = this.onStartHandler.bind(this);
  this.recognition_.onresult = this.onResultHandler.bind(this);
  this.recognition_.onerror = this.onErrorHandler.bind(this);
  this.recognition_.onend = this.onEndHandler.bind(this);
};

VoiceInput.prototype = {
  /**
   * Event handler for mouse/touch down events.
   */
  onDown: function() {
    if (this.recognizing_) {
      this.recognition_.stop();
      return;
    }
    this.recognition_.start();
  },

  /**
   * Speech recognition started. Change microphone key's icon.
   */
  onStartHandler: function() {
    this.recognizing_ = true;
    this.finalResult_ = '';
    if (!this.keyboard_.classList.contains('audio'))
      this.keyboard_.classList.add('audio');
  },

  /**
   * Speech recognizer returns a result.
   * @param{Event} e The SpeechRecognition event that is raised each time
   *     there
   *     are any changes to interim or final results.
   */
  onResultHandler: function(e) {
    for (var i = e.resultIndex; i < e.results.length; i++) {
      if (e.results[i].isFinal)
        this.finalResult_ = e.results[i][0].transcript;
    }
    insertText(this.finalResult_);
  },

  /**
   * Speech recognizer returns an error.
   * @param{Event} e The SpeechRecognitionError event that is raised each time
   *     there is an error.
   */
  onErrorHandler: function(e) {
    console.error('error code = ' + e.error);
  },

  /**
   * Speech recognition ended. Reset microphone key's icon.
   */
  onEndHandler: function() {
    if (this.keyboard_.classList.contains('audio'))
      this.keyboard_.classList.remove('audio');
    this.recognizing_ = false;
  }
};

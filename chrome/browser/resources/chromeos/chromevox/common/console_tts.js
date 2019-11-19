// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to window.console.
 */

goog.provide('ConsoleTts');

goog.require('LogStore');
goog.require('SpeechLog');
goog.require('AbstractTts');
goog.require('TtsInterface');

/**
 * @constructor
 * @implements {TtsInterface}
 */
ConsoleTts = function() {
  /**
   * True if the console TTS is enabled by the user.
   * @type {boolean}
   * @private
   */
  this.enabled_ = false;
};
goog.addSingletonGetter(ConsoleTts);


/** @override */
ConsoleTts.prototype = {
  speak: function(textString, queueMode, properties) {
    if (this.enabled_ && window['console']) {
      var category = TtsCategory.NAV;
      if (properties && properties.category) {
        category = properties.category;
      }

      const speechLog = new SpeechLog(textString, queueMode, category);
      LogStore.getInstance().writeLog(speechLog);
      console.log(speechLog.toString());
    }
    return this;
  },

  /** @override */
  isSpeaking: function() {
    return false;
  },

  /** @override */
  stop: function() {
    if (this.enabled_) {
      console.log('Stop');
    }
  },

  /** @override */
  addCapturingEventListener: function(listener) {},

  /** @override */
  increaseOrDecreaseProperty: function() {},

  /** @override */
  propertyToPercentage: function() {},

  /**
   * Sets the enabled bit.
   * @param {boolean} enabled The new enabled bit.
   */
  setEnabled: function(enabled) {
    this.enabled_ = enabled;
  },

  /** @override */
  getDefaultProperty: function(property) {},

  /** @override */
  toggleSpeechOnOrOff: function() {}
};

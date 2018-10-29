// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to window.console.
 */

goog.provide('ConsoleTts');

goog.require('LogStore');
goog.require('TextLog');
goog.require('cvox.AbstractTts');
goog.require('cvox.TtsInterface');

/**
 * @constructor
 * @implements {cvox.TtsInterface}
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
      var logStr = 'Speak';
      if (queueMode == cvox.QueueMode.FLUSH) {
        logStr += ' (I)';
      } else if (queueMode == cvox.QueueMode.CATEGORY_FLUSH) {
        logStr += ' (C)';
      } else {
        logStr += ' (Q)';
      }
      if (properties && properties.category) {
        logStr += ' category=' + properties.category;
      }
      logStr += ' "' + textString + '"';
      LogStore.getInstance().writeTextLog(logStr, TextLog.LogType.SPEECH);
      console.log(logStr);
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

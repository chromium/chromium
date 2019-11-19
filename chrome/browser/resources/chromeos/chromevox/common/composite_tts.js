// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A composite TTS sends allows ChromeVox to use
 * multiple TTS engines at the same time.
 *
 */

goog.provide('CompositeTts');

goog.require('TtsInterface');

/**
 * A Composite Tts
 * @constructor
 * @implements {TtsInterface}
 */
CompositeTts = function() {
  /**
   * @type {Array<TtsInterface>}
   * @private
   */
  this.ttsEngines_ = [];
};


/**
 * Adds a TTS engine to the composite TTS
 * @param {TtsInterface} tts The TTS to add.
 * @return {CompositeTts} this.
 */
CompositeTts.prototype.add = function(tts) {
  this.ttsEngines_.push(tts);
  return this;
};


/**
 * @override
 */
CompositeTts.prototype.speak = function(textString, queueMode, properties) {
  this.ttsEngines_.forEach(function(engine) {
    engine.speak(textString, queueMode, properties);
  });
  return this;
};


/**
 * Returns true if any of the TTSes are still speaking.
 * @override
 */
CompositeTts.prototype.isSpeaking = function() {
  return this.ttsEngines_.some(function(engine) {
    return engine.isSpeaking();
  });
};


/**
 * @override
 */
CompositeTts.prototype.stop = function() {
  this.ttsEngines_.forEach(function(engine) {
    engine.stop();
  });
};


/**
 * @override
 */
CompositeTts.prototype.addCapturingEventListener = function(listener) {
  this.ttsEngines_.forEach(function(engine) {
    engine.addCapturingEventListener(listener);
  });
};


/**
 * @override
 */
CompositeTts.prototype.increaseOrDecreaseProperty = function(
    propertyName, increase) {
  this.ttsEngines_.forEach(function(engine) {
    engine.increaseOrDecreaseProperty(propertyName, increase);
  });
};


/**
 * @override
 */
CompositeTts.prototype.propertyToPercentage = function(property) {
  for (var i = 0, engine; engine = this.ttsEngines_[i]; i++) {
    var value = engine.propertyToPercentage(property);
    if (value !== undefined) {
      return value;
    }
  }
  return null;
};


/**
 * @override
 */
CompositeTts.prototype.getDefaultProperty = function(property) {
  for (var i = 0, engine; engine = this.ttsEngines_[i]; i++) {
    var value = engine.getDefaultProperty(property);
    if (value !== undefined) {
      return value;
    }
  }
  return null;
};

/** @override */
CompositeTts.prototype.toggleSpeechOnOrOff = function() {
  var value = false;
  this.ttsEngines_.forEach(function(engine) {
    value = value || engine.toggleSpeechOnOrOff();
  });
  return value;
};

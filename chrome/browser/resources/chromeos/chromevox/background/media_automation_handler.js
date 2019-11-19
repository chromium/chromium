// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles media automation events.  Note that to perform any of
 * the actions below such as ducking, and suspension of media sessions, the
 * --enable-audio-focus flag must be passed at the command line.
 */

goog.provide('MediaAutomationHandler');

goog.require('BaseAutomationHandler');
goog.require('TtsCapturingEventListener');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * @constructor
 * @extends {BaseAutomationHandler}
 * @implements {TtsCapturingEventListener}
 */
MediaAutomationHandler = function() {
  /** @type {!Set<AutomationNode>} @private */
  this.mediaRoots_ = new Set();

  /** @type {Date} @private */
  this.lastTtsEvent_ = new Date();

  ChromeVox.tts.addCapturingEventListener(this);

  chrome.automation.getDesktop(function(node) {
    BaseAutomationHandler.call(this, node);

    this.addListener_(
        EventType.MEDIA_STARTED_PLAYING, this.onMediaStartedPlaying);
    this.addListener_(
        EventType.MEDIA_STOPPED_PLAYING, this.onMediaStoppedPlaying);
  }.bind(this));
};

/** @type {number} */
MediaAutomationHandler.MIN_WAITTIME_MS = 1000;

MediaAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /** @override */
  onTtsStart: function() {
    this.lastTtsEvent_ = new Date();
    this.update_({start: true});
  },

  /** @override */
  onTtsEnd: function() {
    var now = new Date();
    setTimeout(function() {
      var then = this.lastTtsEvent_;
      if (now < then) {
        return;
      }
      this.lastTtsEvent_ = now;
      this.update_({end: true});
    }.bind(this), MediaAutomationHandler.MIN_WAITTIME_MS);
  },

  /** @override */
  onTtsInterrupted: function() {
    this.onTtsEnd();
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStartedPlaying: function(evt) {
    this.mediaRoots_.add(evt.target);
    var audioStrategy = localStorage['audioStrategy'];
    if (ChromeVox.tts.isSpeaking() && audioStrategy == 'audioDuck') {
      this.update_({start: true});
    }
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStoppedPlaying: function(evt) {
    // Intentionally does nothing (to cover resume).
  },

  /**
   * Updates the media state for all observed automation roots.
   * @param {{start: (boolean|undefined),
   *          end: (boolean|undefined)}} options
   * @private
   */
  update_: function(options) {
    var it = this.mediaRoots_.values();
    var item = it.next();
    var audioStrategy = localStorage['audioStrategy'];
    while (!item.done) {
      var root = item.value;
      if (options.start) {
        if (audioStrategy == 'audioDuck') {
          root.startDuckingMedia();
        } else if (audioStrategy == 'audioSuspend') {
          root.suspendMedia();
        }
      } else if (options.end) {
        if (audioStrategy == 'audioDuck') {
          root.stopDuckingMedia();
        } else if (audioStrategy == 'audioSuspend') {
          root.resumeMedia();
        }
      }
      item = it.next();
    }
  }
};
});  // goog.scope

new MediaAutomationHandler();

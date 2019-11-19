// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses EarconEngine to play back
 * auditory cues.
 */


goog.provide('NextEarcons');

goog.require('EarconEngine');
goog.require('LogStore');
goog.require('TextLog');
goog.require('AbstractEarcons');


/**
 * @constructor
 * @extends {AbstractEarcons}
 */
NextEarcons = function() {
  AbstractEarcons.call(this);

  if (localStorage['earcons'] === 'false') {
    AbstractEarcons.enabled = false;
  }

  /**
   * @type {EarconEngine}
   * @private
   */
  this.engine_ = new EarconEngine();

  /** @private {boolean} */
  this.shouldPan_ = true;

  if (chrome.audio) {
    chrome.audio.getDevices(
        {isActive: true, streamTypes: [chrome.audio.StreamType.OUTPUT]},
        this.updateShouldPanForDevices_.bind(this));
    chrome.audio.onDeviceListChanged.addListener(
        this.updateShouldPanForDevices_.bind(this));
  } else {
    this.shouldPan_ = false;
  }
};

NextEarcons.prototype = {
  /**
   * @return {string} The human-readable name of the earcon set.
   */
  getName: function() {
    return 'ChromeVox Next earcons';
  },

  /**
   * @override
   */
  playEarcon: function(earcon, opt_location) {
    if (!AbstractEarcons.enabled) {
      return;
    }
    if (localStorage['enableEarconLogging'] == 'true') {
      LogStore.getInstance().writeTextLog(earcon, LogStore.LogType.EARCON);
      console.log('Earcon ' + earcon);
    }
    if (ChromeVoxState.instance.currentRange &&
        ChromeVoxState.instance.currentRange.isValid()) {
      var node = ChromeVoxState.instance.currentRange.start.node;
      var rect = opt_location || node.location;
      var container = node.root.location;
      if (this.shouldPan_) {
        this.engine_.setPositionForRect(rect, container);
      } else {
        this.engine_.resetPan();
      }
    }
    switch (earcon) {
      case Earcon.ALERT_MODAL:
      case Earcon.ALERT_NONMODAL:
        this.engine_.onAlert();
        break;
      case Earcon.BUTTON:
        this.engine_.onButton();
        break;
      case Earcon.CHECK_OFF:
        this.engine_.onCheckOff();
        break;
      case Earcon.CHECK_ON:
        this.engine_.onCheckOn();
        break;
      case Earcon.EDITABLE_TEXT:
        this.engine_.onTextField();
        break;
      case Earcon.INVALID_KEYPRESS:
        this.engine_.onWrap();
        break;
      case Earcon.LINK:
        this.engine_.onLink();
        break;
      case Earcon.LISTBOX:
        this.engine_.onSelect();
        break;
      case Earcon.LIST_ITEM:
      case Earcon.LONG_DESC:
      case Earcon.MATH:
      case Earcon.OBJECT_CLOSE:
      case Earcon.OBJECT_ENTER:
      case Earcon.OBJECT_EXIT:
      case Earcon.OBJECT_OPEN:
      case Earcon.OBJECT_SELECT:
        // TODO(dmazzoni): decide if we want new earcons for these
        // or not. We may choose to not have earcons for some of these.
        break;
      case Earcon.PAGE_FINISH_LOADING:
        this.engine_.cancelProgress();
        break;
      case Earcon.PAGE_START_LOADING:
        this.engine_.startProgress();
        break;
      case Earcon.POP_UP_BUTTON:
        this.engine_.onPopUpButton();
        break;
      case Earcon.RECOVER_FOCUS:
        // TODO(dmazzoni): decide if we want new earcons for this.
        break;
      case Earcon.SELECTION:
        this.engine_.onSelection();
        break;
      case Earcon.SELECTION_REVERSE:
        this.engine_.onSelectionReverse();
        break;
      case Earcon.SKIP:
        this.engine_.onSkim();
        break;
      case Earcon.SLIDER:
        this.engine_.onSlider();
        break;
      case Earcon.WRAP:
      case Earcon.WRAP_EDGE:
        this.engine_.onWrap();
        break;
    }
  },

  /**
   * @override
   */
  cancelEarcon: function(earcon) {
    switch (earcon) {
      case Earcon.PAGE_START_LOADING:
        this.engine_.cancelProgress();
        break;
    }
  },

  /**
   * Updates |this.shouldPan_| based on whether internal speakers are active or
   * not.
   * @param {Array<chrome.audio.AudioDeviceInfo>} devices
   * @private
   */
  updateShouldPanForDevices_: function(devices) {
    this.shouldPan_ = !devices.some((device) => {
      return device.isActive &&
          device.deviceType == chrome.audio.DeviceType.INTERNAL_SPEAKER;
    });
  },
};

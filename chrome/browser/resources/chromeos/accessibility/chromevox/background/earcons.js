// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses EarconEngine to play back
 * auditory cues.
 */

import {LocalStorage} from '../../common/local_storage.js';
import {EarconId} from '../common/earcon_id.js';
import {LogType} from '../common/log_types.js';

import {AbstractEarcons} from './abstract_earcons.js';
import {ChromeVoxRange} from './chromevox_range.js';
import {EarconEngine} from './earcon_engine.js';
import {LogStore} from './logging/log_store.js';

export class Earcons extends AbstractEarcons {
  constructor() {
    super();

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
          devices => this.updateShouldPanForDevices_(devices));
      chrome.audio.onDeviceListChanged.addListener(
          devices => this.updateShouldPanForDevices_(devices));
    } else {
      this.shouldPan_ = false;
    }
  }

  /**
   * @return {string} The human-readable name of the earcon set.
   */
  getName() {
    return 'ChromeVox earcons';
  }

  /**
   * Plays the specified earcon sound.
   * @param {EarconId} earcon An earcon identifier.
   * @param {chrome.automation.Rect=} opt_location A location associated with
   *     the earcon such as a control's bounding rectangle.
   * @override
   */
  playEarcon(earcon, opt_location) {
    if (!this.enabled) {
      return;
    }
    if (LocalStorage.get('enableEarconLogging')) {
      LogStore.instance.writeTextLog(earcon, LogType.EARCON);
      console.log('Earcon ' + earcon);
    }
    if (ChromeVoxRange.current && ChromeVoxRange.current.isValid()) {
      const node = ChromeVoxRange.current.start.node;
      const rect = opt_location || node.location;
      const container = node.root.location;
      if (this.shouldPan_) {
        this.engine_.setPositionForRect(rect, container);
      } else {
        this.engine_.resetPan();
      }
    }

    this.engine_.playEarcon(earcon);
  }

  /**
   * @override
   */
  cancelEarcon(earcon) {
    switch (earcon) {
      case EarconId.PAGE_START_LOADING:
        this.engine_.cancelProgress();
        break;
    }
  }

  /**
   * Updates |this.shouldPan_| based on whether internal speakers are active
   * or not.
   * @param {Array<chrome.audio.AudioDeviceInfo>} devices
   * @private
   */
  updateShouldPanForDevices_(devices) {
    this.shouldPan_ = !devices.some(device => {
      return device.isActive &&
          device.deviceType === chrome.audio.DeviceType.INTERNAL_SPEAKER;
    });
  }
}

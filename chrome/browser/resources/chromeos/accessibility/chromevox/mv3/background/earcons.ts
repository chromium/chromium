// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses EarconEngine to play back
 * auditory cues.
 */

import {EarconId} from '../common/earcon_id.js';
import {LogType} from '../common/log_types.js';
import {Msgs} from '../common/msgs.js';
import {OffscreenBridge} from '../common/offscreen_bridge.js';
import {SettingsManager} from '../common/settings_manager.js';
import {Personality, QueueMode} from '../common/tts_types.js';

import {AbstractEarcons} from './abstract_earcons.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxRange} from './chromevox_range.js';
import {LogStore} from './logging/log_store.js';

const DeviceType = chrome.audio.DeviceType;
type AudioDeviceInfo = chrome.audio.AudioDeviceInfo;
type Rect = chrome.automation.Rect;

/**
 * High-level class that manages when each earcon should start (and when
 * relevant, stop) playing.
 */
export class Earcons extends AbstractEarcons {
  private shouldPan_ = true;

  constructor() {
    super();

    if (chrome.audio) {
      chrome.audio.getDevices(
          {isActive: true, streamTypes: [chrome.audio.StreamType.OUTPUT]},
          (devices: AudioDeviceInfo[]) =>
              this.updateShouldPanForDevices_(devices));
      chrome.audio.onDeviceListChanged.addListener(
          (devices: AudioDeviceInfo[]) =>
              this.updateShouldPanForDevices_(devices));
    } else {
      this.shouldPan_ = false;
    }
  }

  /**
   * @return The human-readable name of the earcon set.
   */
  getName(): string {
    return 'ChromeVox earcons';
  }

  /**
   * Plays the specified earcon sound.
   * @param {EarconId} earcon An earcon identifier.
   * @param {chrome.automation.Rect=} opt_location A location associated with
   *     the earcon such as a control's bounding rectangle.
   */
  override playEarcon(earcon: EarconId, opt_location?: Rect): void {
    if (!this.enabled) {
      return;
    }
    if (SettingsManager.getBoolean('enableEarconLogging')) {
      LogStore.instance.writeTextLog(earcon, LogType.EARCON);
      console.log('Earcon ' + earcon);
    }
    if (ChromeVoxRange.current?.isValid()) {
      const node = ChromeVoxRange.current.start.node;
      const rect = opt_location ?? node.location;
      const container = node.root?.location;
      if (this.shouldPan_ && container) {
        OffscreenBridge.earconSetPositionForRect(rect, container);
      } else {
        OffscreenBridge.earconCancelProgress();
      }
    }

    OffscreenBridge.playEarcon(earcon);
  }

  override cancelEarcon(earcon: EarconId): void {
    switch (earcon) {
      case EarconId.PAGE_START_LOADING:
        OffscreenBridge.earconCancelProgress();
        break;
      case EarconId.CHROMEVOX_LOADING:
        OffscreenBridge.earconCancelLoading();
        break;
    }
  }

  override toggle(): void {
    this.enabled = !this.enabled;
    const announce =
        this.enabled ? Msgs.getMsg('earcons_on') : Msgs.getMsg('earcons_off');
    ChromeVox.tts.speak(announce, QueueMode.FLUSH, Personality.ANNOTATION);
  }

  /**
   * Updates |this.shouldPan_| based on whether internal speakers are active
   * or not.
   * @param devices
   */
  private updateShouldPanForDevices_(devices: AudioDeviceInfo[]): void {
    this.shouldPan_ = !devices.some(
        (device: AudioDeviceInfo) => device.isActive &&
            device.deviceType === DeviceType.INTERNAL_SPEAKER);
  }
}

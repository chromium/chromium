// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from '../../device/camera3_device_info.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as localStorage from '../../models/local_storage.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import {Facing, PerfEvent, ViewName} from '../../type.js';
import * as util from '../../util.js';

/**
 * Creates a controller for the options of Camera view.
 */
export class Options {
  /**
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {function(): !Promise} doSwitchDevice Callback to trigger device
   *     switching.
   */
  constructor(infoUpdater, doSwitchDevice) {
    /**
     * @type {!DeviceInfoUpdater}
     * @private
     * @const
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {function(): !Promise}
     * @private
     * @const
     */
    this.doSwitchDevice_ = doSwitchDevice;

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMic_ = dom.get('#toggle-mic', HTMLInputElement);

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMirror_ = dom.get('#toggle-mirror', HTMLInputElement);

    /**
     * Device id of the camera device currently used or selected.
     * @type {?string}
     * @private
     */
    this.videoDeviceId_ = null;

    /**
     * Whether list of video devices is being refreshed now.
     * @type {boolean}
     * @private
     */
    this.refreshingVideoDeviceIds_ = false;

    /**
     * Mirroring set per device.
     * @type {!Object}
     * @private
     */
    this.mirroringToggles_ = {};

    /**
     * Current audio track in use.
     * @type {?MediaStreamTrack}
     * @private
     */
    this.audioTrack_ = null;

    dom.get('#switch-device', HTMLButtonElement)
        .addEventListener('click', () => this.switchDevice_());
    dom.get('#open-settings', HTMLButtonElement)
        .addEventListener('click', () => nav.open(ViewName.SETTINGS));

    this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
    this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

    util.bindElementAriaLabelWithState({
      element: dom.get('#toggle-timer', Element),
      state: state.State.TIMER_3SEC,
      onLabel: I18nString.TOGGLE_TIMER_3S_BUTTON,
      offLabel: I18nString.TOGGLE_TIMER_10S_BUTTON,
    });

    // Restore saved mirroring states per video device.
    this.mirroringToggles_ = localStorage.getObject('mirroringToggles');
    // Remove the deprecated values.
    localStorage.remove('effectIndex', 'toggleMulti', 'toggleMirror');

    this.infoUpdater_.addDeviceChangeListener((updater) => {
      state.set(state.State.MULTI_CAMERA, updater.getDevicesInfo().length >= 2);
    });
  }

  /**
   * Device id of the camera device currently used or selected.
   * @return {?string}
   */
  get currentDeviceId() {
    return this.videoDeviceId_;
  }

  /**
   * Switches to the next available camera device.
   * @private
   */
  async switchDevice_() {
    if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return;
    }
    state.set(PerfEvent.CAMERA_SWITCHING, true);
    const devices = await this.infoUpdater_.getDevicesInfo();
    animate.play(dom.get('#switch-device', HTMLElement));
    let index =
        devices.findIndex((entry) => entry.deviceId === this.videoDeviceId_);
    if (index === -1) {
      index = 0;
    }
    if (devices.length > 0) {
      index = (index + 1) % devices.length;
      this.videoDeviceId_ = devices[index].deviceId;
    }
    const isSuccess = await this.doSwitchDevice_();
    state.set(PerfEvent.CAMERA_SWITCHING, false, {hasError: !isSuccess});
  }

  /**
   * Updates the options' values for the current constraints and stream.
   * @param {!MediaStream} stream Current Stream in use.
   * @param {!Facing} facing
   */
  updateValues(stream, facing) {
    const track = stream.getVideoTracks()[0];
    const trackSettings = track.getSettings && track.getSettings();
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
    this.updateMirroring_(facing);
    this.audioTrack_ = stream.getAudioTracks()[0];
    this.updateAudioByMic_();
  }

  /**
   * Updates mirroring for a new stream.
   * @param {!Facing} facing Facing of the stream.
   * @private
   */
  updateMirroring_(facing) {
    // Update mirroring by detected facing-mode. Enable mirroring by default if
    // facing-mode isn't available.
    let enabled = facing !== Facing.ENVIRONMENT;

    // Override mirroring only if mirroring was toggled manually.
    if (this.videoDeviceId_ in this.mirroringToggles_) {
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    }

    util.toggleChecked(this.toggleMirror_, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   * @private
   */
  saveMirroring_() {
    this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
    localStorage.set('mirroringToggles', this.mirroringToggles_);
  }

  /**
   * Enables/disables the current audio track according to the microphone
   * option.
   * @private
   */
  updateAudioByMic_() {
    if (this.audioTrack_) {
      this.audioTrack_.enabled = this.toggleMic_.checked;
    }
  }

  /**
   * Gets the video device ids sorted by preference.
   * @param {!Facing} facing Preferred facing to use
   * @return {!Array<string>}
   */
  videoDeviceIds(facing) {
    /** @type {!Array<(!Camera3DeviceInfo|!MediaDeviceInfo)>} */
    let devices;
    /**
     * Object mapping from device id to facing. Set to null for fake cameras.
     * @type {?Object<string, !Facing>}
     */
    let facings = null;

    const camera3Info = this.infoUpdater_.getCamera3DevicesInfo();
    if (camera3Info) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
    } else {
      devices = this.infoUpdater_.getDevicesInfo();
    }

    const preferredFacing =
        facing === Facing.NOT_SET ? util.getDefaultFacing() : facing;
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.videoDeviceId_ ? a === this.videoDeviceId_ :
                                (facings && facings[a] === preferredFacing)) {
        return -1;
      }
      return 1;
    });
    return sorted;
  }
}

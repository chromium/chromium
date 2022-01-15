// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as localStorage from '../../models/local_storage.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import {Facing, ViewName} from '../../type.js';
import * as util from '../../util.js';

/**
 * Creates a controller for the options of Camera view.
 */
export class Options {
  private readonly toggleMic = dom.get('#toggle-mic', HTMLInputElement);
  private readonly toggleMirror = dom.get('#toggle-mirror', HTMLInputElement);

  /**
   * Device id of the camera device currently used or selected.
   */
  private videoDeviceId: string|null = null;

  /**
   * Mirroring set per device.
   */
  private mirroringToggles: Record<string, boolean> = {};

  /**
   * Current audio track in use.
   */
  private audioTrack: MediaStreamTrack|null = null;

  /**
   * @param doSwitchDevice Callback to trigger device switching.
   */
  constructor(
      private readonly infoUpdater: DeviceInfoUpdater,
      private readonly doSwitchDevice: () => Promise<void>| null,
  ) {
    dom.get('#switch-device', HTMLButtonElement)
        .addEventListener('click', () => {
          const switching = this.doSwitchDevice();
          if (switching !== null) {
            animate.play(dom.get('#switch-device', HTMLElement));
          }
        });
    dom.get('#open-settings', HTMLButtonElement)
        .addEventListener('click', () => nav.open(ViewName.SETTINGS));

    this.toggleMic.addEventListener('click', () => this.updateAudioByMic());
    this.toggleMirror.addEventListener('click', () => this.saveMirroring());

    util.bindElementAriaLabelWithState({
      element: dom.get('#toggle-timer', Element),
      state: state.State.TIMER_3SEC,
      onLabel: I18nString.TOGGLE_TIMER_3S_BUTTON,
      offLabel: I18nString.TOGGLE_TIMER_10S_BUTTON,
    });

    // Restore saved mirroring states per video device.
    this.mirroringToggles = localStorage.getObject('mirroringToggles');
    // Remove the deprecated values.
    localStorage.remove('effectIndex', 'toggleMulti', 'toggleMirror');

    this.infoUpdater.addDeviceChangeListener((updater) => {
      state.set(state.State.MULTI_CAMERA, updater.getDevicesInfo().length >= 2);
    });
  }

  /**
   * Updates the options' values for the current constraints and stream.
   * @param stream Current Stream in use.
   */
  updateValues(stream: MediaStream, deviceId: string, facing: Facing): void {
    this.videoDeviceId = deviceId;
    this.updateMirroring(facing);
    this.audioTrack = stream.getAudioTracks()[0];
    this.updateAudioByMic();
  }

  /**
   * Updates mirroring for a new stream.
   * @param facing Facing of the stream.
   */
  private updateMirroring(facing: Facing) {
    // Update mirroring by detected facing-mode. Enable mirroring by default if
    // facing-mode isn't available.
    let enabled = facing !== Facing.ENVIRONMENT;

    // Override mirroring only if mirroring was toggled manually.
    if (this.videoDeviceId in this.mirroringToggles) {
      enabled = this.mirroringToggles[this.videoDeviceId];
    }

    util.toggleChecked(this.toggleMirror, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   */
  private saveMirroring() {
    this.mirroringToggles[this.videoDeviceId] = this.toggleMirror.checked;
    localStorage.set('mirroringToggles', this.mirroringToggles);
  }

  /**
   * Enables/disables the current audio track according to the microphone
   * option.
   */
  private updateAudioByMic() {
    if (this.audioTrack) {
      this.audioTrack.enabled = this.toggleMic.checked;
    }
  }
}

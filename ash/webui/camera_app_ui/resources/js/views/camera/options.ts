// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
import {assert, assertInstanceof} from '../../assert.js';
import {
  CameraConfig,
  CameraInfo,
  CameraManager,
  CameraUI,
} from '../../device/index.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as localStorage from '../../models/local_storage.js';
import * as nav from '../../nav.js';
import * as newFeatureToast from '../../new_feature_toast.js';
import * as state from '../../state.js';
import {Facing, Mode, Resolution, ViewName} from '../../type.js';
import * as util from '../../util.js';
import {PTZPanelOptions} from '../view.js';

/**
 * All supported constant fps options of video recording.
 */
const SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Creates a controller for the options of Camera view.
 */
export class Options implements CameraUI {
  private readonly toggleMic = dom.get('#toggle-mic', HTMLInputElement);

  private readonly toggleMirror = dom.get('#toggle-mirror', HTMLInputElement);

  private readonly toggleFps = dom.get('#toggle-fps', HTMLInputElement);

  private readonly openPTZPanel = dom.get('#open-ptz-panel', HTMLButtonElement);

  private readonly switchDeviceButton =
      dom.get('#switch-device', HTMLButtonElement);

  /**
   * CameraConfig of the camera device currently used or selected.
   */
  private currentConfig: CameraConfig|null = null;

  /**
   * Mirroring set per device.
   */
  private mirroringToggles: Record<string, boolean> = {};

  /**
   * Current audio track in use.
   */
  private audioTrack: MediaStreamTrack|null = null;

  private cameraAvailable = false;

  /**
   * @param cameraManager Camera manager instance.
   */
  constructor(private readonly cameraManager: CameraManager) {
    this.cameraManager.registerCameraUI(this);
    this.switchDeviceButton.addEventListener('click', () => {
      if (state.get(state.State.TAKING) || !this.cameraAvailable) {
        return;
      }
      const switching = this.cameraManager.switchCamera();
      if (switching !== null) {
        animate.play(dom.get('#switch-device', HTMLElement));
      }
    });
    dom.get('#open-settings', HTMLButtonElement)
        .addEventListener('click', () => nav.open(ViewName.SETTINGS));

    this.toggleMic.addEventListener('click', () => this.updateAudioByMic());
    this.toggleMirror.addEventListener('click', () => this.saveMirroring());

    this.initOpenPTZPanel();

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

    state.addObserver(state.State.TAKING, () => {
      this.updateOptionAvailability();
    });
    this.toggleFps.addEventListener('click', (e) => {
      if (state.get(state.State.TAKING) || !this.cameraAvailable) {
        e.preventDefault();
        return;
      }
    });
    this.toggleFps.addEventListener('change', () => {
      if (this.currentConfig === null) {
        return;
      }
      const prefFps = this.toggleFps.checked ? 60 : 30;
      this.updateVideoConstFpsOption(prefFps);
      const resolution = assertInstanceof(
          this.cameraManager.getCaptureResolution(), Resolution);
      const reconfiguring = this.cameraManager.setPrefVideoConstFps(
          this.currentConfig.deviceId, resolution, prefFps);
      if (reconfiguring === null) {
        return;
      }
      state.set(state.State.MODE_SWITCHING, true);
      (async () => {
        const hasError = !await reconfiguring;
        state.set(state.State.MODE_SWITCHING, false, {hasError});
      })();
    });
  }

  private initOpenPTZPanel() {
    this.openPTZPanel.addEventListener('click', () => {
      nav.open(ViewName.PTZ_PANEL, new PTZPanelOptions({
                 stream: this.cameraManager.getPreviewVideo().getStream(),
                 vidPid: this.cameraManager.getVidPid(),
                 resetPTZ: () => this.cameraManager.resetPTZ(),
               }));
      highlight(false);
    });

    // Highlight effect for PTZ button.
    let toastShown = false;
    const highlight = (enabled: boolean) => {
      if (!enabled) {
        if (toastShown) {
          newFeatureToast.hide();
          toastShown = false;
        }
        return;
      }
      toastShown = true;
      newFeatureToast.show(this.openPTZPanel);
      newFeatureToast.focus();
    };

    this.cameraManager.registerCameraUI({
      onUpdateConfig: () => {
        const ptzToastKey = 'isPTZToastShown';
        if (!state.get(state.State.ENABLE_PTZ) ||
            state.get(state.State.IS_NEW_FEATURE_TOAST_SHOWN) ||
            localStorage.getBool(ptzToastKey)) {
          highlight(false);
          return;
        }
        localStorage.set(ptzToastKey, true);
        state.set(state.State.IS_NEW_FEATURE_TOAST_SHOWN, true);
        highlight(true);
      },
    });
  }

  private updateVideoConstFpsOption(prefFps: number|null) {
    this.toggleFps.checked = prefFps === 60;
    for (const fps of SUPPORTED_CONSTANT_FPS) {
      state.set(state.assertState(`fps-${fps}`), fps === prefFps);
    }
  }

  onUpdateCapability(cameraInfo: CameraInfo): void {
    state.set(state.State.MULTI_CAMERA, cameraInfo.devicesInfo.length >= 2);
  }

  onUpdateConfig(config: CameraConfig): void {
    this.currentConfig = config;
    this.updateMirroring();
    this.audioTrack = this.cameraManager.getAudioTrack();
    this.updateAudioByMic();

    for (const fps of SUPPORTED_CONSTANT_FPS) {
      state.set(
          state.assertState(`fps-${fps}`),
          fps === this.cameraManager.getConstFps());
    }

    this.toggleFps.hidden = (() => {
      if (config.mode !== Mode.VIDEO) {
        return true;
      }
      if (config.facing !== Facing.EXTERNAL) {
        return true;
      }
      if (this.currentConfig === null) {
        return true;
      }
      const info = this.cameraManager.getCameraInfo().getCamera3DeviceInfo(
          this.currentConfig.deviceId);
      if (info === null) {
        return true;
      }
      const constFpses = info.fpsRanges.filter(
          ({minFps, maxFps}) =>
              minFps === maxFps && SUPPORTED_CONSTANT_FPS.includes(minFps));
      return constFpses.length <= 1;
    })();
    if (!this.toggleFps.hidden) {
      this.updateVideoConstFpsOption(this.cameraManager.getConstFps());
    }
  }

  onCameraAvailable(): void {
    this.cameraAvailable = true;
    this.updateOptionAvailability();
  }

  onCameraUnavailable(): void {
    this.cameraAvailable = false;
    this.updateOptionAvailability();
  }

  private updateOptionAvailability(): void {
    this.toggleMirror.disabled = !this.allowModifyMirrorState();
    this.toggleFps.disabled =
        !this.cameraAvailable || state.get(state.State.TAKING);
  }

  /**
   * Returns whether the mirror state can be modified. We don't allow toggling
   * mirror button when it is under scan mode unless it is an external camera
   * since we don't know how the external camera will be used.
   */
  private allowModifyMirrorState(): boolean {
    assert(this.currentConfig !== null);
    return this.currentConfig.mode !== Mode.SCAN ||
        this.currentConfig.facing === Facing.EXTERNAL;
  }

  /**
   * Updates mirroring for a new stream.
   */
  private updateMirroring() {
    assert(this.currentConfig !== null);
    // Update mirroring by detected facing-mode. Enable mirroring by default if
    // facing-mode isn't available.
    let enabled = this.currentConfig.facing !== Facing.ENVIRONMENT;

    const deviceId = this.currentConfig.deviceId;
    // Override mirroring only if mirroring was toggled manually.
    if (deviceId in this.mirroringToggles && this.allowModifyMirrorState()) {
      enabled = this.mirroringToggles[deviceId];
    }
    util.toggleChecked(this.toggleMirror, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   */
  private saveMirroring() {
    if (this.currentConfig !== null) {
      this.mirroringToggles[this.currentConfig.deviceId] =
          this.toggleMirror.checked;
      localStorage.set('mirroringToggles', this.mirroringToggles);
    }
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

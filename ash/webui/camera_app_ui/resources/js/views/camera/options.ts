// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
import {assert, assertExists} from '../../assert.js';
import {
  CameraConfig,
  CameraInfo,
  CameraManager,
  CameraUi,
} from '../../device/index.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as localStorage from '../../models/local_storage.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import {Facing, LocalStorageKey, Mode, ViewName} from '../../type.js';
import * as util from '../../util.js';
import {OptionPanelOptions, PtzPanelOptions, StateOption} from '../view.js';

/**
 * Creates a controller for the options of Camera view.
 */
export class Options implements CameraUi {
  private readonly toggleMic = dom.get('#toggle-mic', HTMLButtonElement);

  private readonly openMirrorPanel =
      dom.get('#open-mirror-panel', HTMLButtonElement);

  private readonly openGridPanel =
      dom.get('#open-grid-panel', HTMLButtonElement);

  private readonly openTimerPanel =
      dom.get('#open-timer-panel', HTMLButtonElement);

  private readonly openPTZPanel = dom.get('#open-ptz-panel', HTMLButtonElement);

  private readonly switchDeviceButton =
      dom.get('switch-device-button', HTMLElement);

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

  constructor(private readonly cameraManager: CameraManager) {
    this.cameraManager.registerCameraUi(this);
    this.switchDeviceButton.addEventListener('click', () => {
      if (state.get(state.State.TAKING)) {
        return;
      }
      const switching = this.cameraManager.switchCamera();
      if (switching !== null) {
        animate.play(this.switchDeviceButton);
      }
    });
    dom.get('#open-settings', HTMLButtonElement)
        .addEventListener('click', () => nav.open(ViewName.SETTINGS));

    this.initOpenMirrorPanel();
    this.initOpenGridPanel();
    this.initOpenTimerPanel();
    this.initOpenPtzPanel();
    this.initToggleMic();

    // Restore saved mirroring states per video device.
    this.mirroringToggles =
        localStorage.getObject(LocalStorageKey.MIRRORING_TOGGLES);
  }

  private setAriaLabelForOptionButton(
      element: HTMLElement, titleLabel: I18nString, stateOptions: StateOption[],
      ariaDescribedByElement: HTMLElement) {
    element.setAttribute('i18n-label', titleLabel);
    for (const {ariaLabel, state: targetState, isDisableOption = false} of
             stateOptions) {
      const stateEnabled = state.get(targetState);
      if ((stateEnabled && !isDisableOption) ||
          (!stateEnabled && isDisableOption)) {
        ariaDescribedByElement.setAttribute('i18n-text', ariaLabel);
        util.setupI18nElements(ariaDescribedByElement);
        break;
      }
    }
    util.setupI18nElements(element);
  }

  private initOpenMirrorPanel() {
    const stateOptions = [
      {
        label: I18nString.LABEL_OFF,
        ariaLabel: I18nString.ARIA_MIRROR_OFF,
        state: state.State.MIRROR,
        isDisableOption: true,
      },
      {
        label: I18nString.LABEL_ON,
        ariaLabel: I18nString.ARIA_MIRROR_ON,
        state: state.State.MIRROR,
      },
    ];
    const titleLabel = I18nString.OPEN_MIRROR_PANEL_BUTTON;
    const ariaDescribedByElement =
        this.createAriaDescribedByElement(this.openMirrorPanel);
    this.setAriaLabelForOptionButton(
        this.openMirrorPanel, titleLabel, stateOptions, ariaDescribedByElement);
    this.openMirrorPanel.addEventListener('click', () => {
      nav.open(ViewName.OPTION_PANEL, new OptionPanelOptions({
                 triggerButton: this.openMirrorPanel,
                 titleLabel,
                 stateOptions,
                 onStateChanged: (newState) => {
                   const enabled = newState !== null;
                   state.set(state.State.MIRROR, enabled);
                   this.saveMirroring(enabled);
                 },
                 ariaDescribedByElement,
               }));
    });
  }

  private initOpenGridPanel() {
    const stateOptions = [
      {
        label: I18nString.LABEL_OFF,
        ariaLabel: I18nString.ARIA_GRID_OFF,
        state: state.State.GRID,
        isDisableOption: true,
      },
      {
        label: I18nString.LABEL_GRID_3X3,
        ariaLabel: I18nString.ARIA_GRID_3X3,
        state: state.State.GRID_3x3,
      },
      {
        label: I18nString.LABEL_GRID_4X4,
        ariaLabel: I18nString.ARIA_GRID_4X4,
        state: state.State.GRID_4x4,
      },
      {
        label: I18nString.LABEL_GRID_GOLDEN,
        ariaLabel: I18nString.LABEL_GRID_GOLDEN,
        state: state.State.GRID_GOLDEN,
      },
    ];
    const titleLabel = I18nString.OPEN_GRID_PANEL_BUTTON;
    const ariaDescribedByElement =
        this.createAriaDescribedByElement(this.openGridPanel);
    this.setAriaLabelForOptionButton(
        this.openGridPanel, titleLabel, stateOptions, ariaDescribedByElement);
    this.openGridPanel.addEventListener('click', () => {
      nav.open(ViewName.OPTION_PANEL, new OptionPanelOptions({
                 triggerButton: this.openGridPanel,
                 titleLabel,
                 stateOptions,
                 onStateChanged: (newState) => {
                   state.set(state.State.GRID, newState !== null);
                   for (const s
                            of [state.State.GRID_3x3, state.State.GRID_4x4,
                                state.State.GRID_GOLDEN]) {
                     state.set(s, newState === s);
                   }
                 },
                 ariaDescribedByElement,
               }));
    });
  }

  private initOpenTimerPanel() {
    const stateOptions = [
      {
        label: I18nString.LABEL_OFF,
        ariaLabel: I18nString.ARIA_TIMER_OFF,
        state: state.State.TIMER,
        isDisableOption: true,
      },
      {
        label: I18nString.LABEL_TIMER_3S,
        ariaLabel: I18nString.ARIA_TIMER_3S,
        state: state.State.TIMER_3SEC,
      },
      {
        label: I18nString.LABEL_TIMER_10S,
        ariaLabel: I18nString.ARIA_TIMER_10S,
        state: state.State.TIMER_10SEC,
      },
    ];
    const titleLabel = I18nString.OPEN_TIMER_PANEL_BUTTON;
    const ariaDescribedByElement =
        this.createAriaDescribedByElement(this.openTimerPanel);
    this.setAriaLabelForOptionButton(
        this.openTimerPanel, titleLabel, stateOptions, ariaDescribedByElement);
    this.openTimerPanel.addEventListener('click', () => {
      nav.open(
          ViewName.OPTION_PANEL, new OptionPanelOptions({
            triggerButton: this.openTimerPanel,
            titleLabel,
            stateOptions,
            onStateChanged: (newState) => {
              state.set(state.State.TIMER, newState !== null);
              for (const s
                       of [state.State.TIMER_3SEC, state.State.TIMER_10SEC]) {
                state.set(s, newState === s);
              }
            },
            ariaDescribedByElement,
          }));
    });
  }

  private initOpenPtzPanel() {
    this.openPTZPanel.addEventListener('click', () => {
      toggleIndicatorOnOpenPTZButton(false);
      nav.open(
          ViewName.PTZ_PANEL,
          new PtzPanelOptions(this.cameraManager.getPtzController()));
    });
  }

  private initToggleMic() {
    const updateMicState = (newMicState: boolean) => {
      state.set(state.State.MIC, newMicState);
      // The checked state is whether the mic is muted or not, which is the
      // inverse of whether the mic is enabled.
      this.toggleMic.ariaChecked = newMicState ? 'false' : 'true';
      this.updateAudioByMic();
    };
    updateMicState(localStorage.getBool(LocalStorageKey.TOGGLE_MIC, true));
    this.toggleMic.addEventListener('click', () => {
      const newMicState = !state.get(state.State.MIC);
      updateMicState(newMicState);
      localStorage.set(LocalStorageKey.TOGGLE_MIC, newMicState);
    });
    // The label on/off state is whether the mic is muted or not, which is also
    // the inverse of whether the mic is enabled.
    util.bindElementAriaLabelWithState({
      element: this.toggleMic,
      state: state.State.MIC,
      onLabel: I18nString.ARIA_MUTE_OFF,
      offLabel: I18nString.ARIA_MUTE_ON,
    });
  }

  onUpdateCapability(cameraInfo: CameraInfo): void {
    state.set(state.State.MULTI_CAMERA, cameraInfo.devicesInfo.length >= 2);
  }

  onUpdateConfig(config: CameraConfig): void {
    this.currentConfig = config;
    this.updateMirroring();
    this.updateOptionAvailability();
    this.audioTrack = this.cameraManager.getAudioTrack();
    this.updateAudioByMic();
  }

  private updateOptionAvailability(): void {
    this.openMirrorPanel.disabled = !this.allowModifyMirrorState();
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

    state.set(state.State.MIRROR, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   *
   * @param enabled Whether the mirroring is enabled.
   */
  private saveMirroring(enabled: boolean) {
    if (this.currentConfig !== null) {
      this.mirroringToggles[this.currentConfig.deviceId] = enabled;
      localStorage.set(
          LocalStorageKey.MIRRORING_TOGGLES, this.mirroringToggles);
    }
  }

  /**
   * Enables/disables the current audio track according to the microphone
   * option.
   */
  private updateAudioByMic() {
    if (this.audioTrack !== null) {
      this.audioTrack.enabled = state.get(state.State.MIC);
    }
  }

  /**
   * Creates an element as `triggerButton`'s aria-describedby reference. The id
   * of the created element is the ID of `triggerButton` with the suffix
   * "-desc".
   */
  private createAriaDescribedByElement(triggerButton: HTMLElement) {
    const element = document.createElement('div');
    const parent = assertExists(triggerButton.parentElement);
    element.id = `${triggerButton.id}-desc`;
    element.hidden = true;
    parent.insertBefore(element, triggerButton);
    triggerButton.setAttribute('aria-describedby', element.id);
    return element;
  }
}

/**
 * Toggles to show or hide the indicator icon that is used to notify users about
 * the new super-resolution feature.
 */
export function toggleIndicatorOnOpenPTZButton(display: boolean): void {
  const openPtzPanel = dom.get('#open-ptz-panel', HTMLButtonElement);
  openPtzPanel.classList.toggle('notify-new-feature', display);
}

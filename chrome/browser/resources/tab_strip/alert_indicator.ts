// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './alert_indicator.html.js';
import {TabAlertState} from './tabs.mojom-webui.js';

const MAX_WIDTH: string = '16px';

function getAriaLabel(alertState: TabAlertState): string {
  // The existing labels for alert states currently expects to format itself
  // using the title of the tab (eg. "Website - Audio is playing"). The WebUI
  // tab strip will provide the title of the tab elsewhere outside of this
  // element, so just provide an empty string as the title here. This also
  // allows for multiple labels for the same title (eg. "Website - Audio is
  // playing - VR is presenting").
  switch (alertState) {
    case TabAlertState.kMediaRecording:
      return loadTimeData.getStringF('mediaRecording', '');
    case TabAlertState.kAudioRecording:
      return loadTimeData.getStringF('audioRecording', '');
    case TabAlertState.kVideoRecording:
      return loadTimeData.getStringF('videoRecording', '');
    case TabAlertState.kTabCapturing:
      return loadTimeData.getStringF('tabCapturing', '');
    case TabAlertState.kAudioPlaying:
      return loadTimeData.getStringF('audioPlaying', '');
    case TabAlertState.kAudioMuting:
      return loadTimeData.getStringF('audioMuting', '');
    case TabAlertState.kBluetoothConnected:
      return loadTimeData.getStringF('bluetoothConnected', '');
    case TabAlertState.kUsbConnected:
      return loadTimeData.getStringF('usbConnected', '');
    case TabAlertState.kHidConnected:
      return loadTimeData.getStringF('hidConnected', '');
    case TabAlertState.kSerialConnected:
      return loadTimeData.getStringF('serialConnected', '');
    case TabAlertState.kPipPlaying:
      return loadTimeData.getStringF('pipPlaying', '');
    case TabAlertState.kDesktopCapturing:
      return loadTimeData.getStringF('desktopCapturing', '');
    case TabAlertState.kVrPresentingInHeadset:
      return loadTimeData.getStringF('vrPresenting', '');
    default:
      return '';
  }
}

const ALERT_STATE_MAP: Map<TabAlertState, string> = new Map([
  [TabAlertState.kMediaRecording, 'media-recording'],
  [TabAlertState.kAudioRecording, 'audio-recording'],
  [TabAlertState.kVideoRecording, 'video-recording'],
  [TabAlertState.kTabCapturing, 'tab-capturing'],
  [TabAlertState.kAudioPlaying, 'audio-playing'],
  [TabAlertState.kAudioMuting, 'audio-muting'],
  [TabAlertState.kBluetoothConnected, 'bluetooth-connected'],
  [TabAlertState.kUsbConnected, 'usb-connected'],
  [TabAlertState.kHidConnected, 'hid-connected'],
  [TabAlertState.kSerialConnected, 'serial-connected'],
  [TabAlertState.kPipPlaying, 'pip-playing'],
  [TabAlertState.kDesktopCapturing, 'desktop-capturing'],
  [TabAlertState.kVrPresentingInHeadset, 'vr-presenting'],
]);

/**
 * Use for mapping to CSS attributes.
 */
function getAlertStateAttribute(alertState: TabAlertState): string {
  return ALERT_STATE_MAP.get(alertState) || '';
}

export class AlertIndicatorElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private alertState_: TabAlertState|null = null;
  private fadeDurationMs_: number = 125;
  private fadeInAnimation_: Animation|null;
  private fadeOutAnimation_: Animation|null;
  private fadeOutAnimationPromise_: Promise<void>|null;

  constructor() {
    super();

    /**
     * An animation that is currently in-flight to fade the element in.
     */
    this.fadeInAnimation_ = null;

    /**
     * An animation that is currently in-flight to fade the element out.
     */
    this.fadeOutAnimation_ = null;

    /**
     * A promise that resolves when the fade out animation finishes or rejects
     * if a fade out animation is canceled.
     */
    this.fadeOutAnimationPromise_ = null;
  }

  get alertState(): TabAlertState {
    assert(this.alertState_ !== null);
    return this.alertState_;
  }

  set alertState(alertState: TabAlertState) {
    this.setAttribute('alert-state_', getAlertStateAttribute(alertState));
    this.setAttribute('aria-label', getAriaLabel(alertState));
    this.alertState_ = alertState;
  }

  overrideFadeDurationForTesting(duration: number) {
    this.fadeDurationMs_ = duration;
  }

  show() {
    if (this.fadeOutAnimation_) {
      // Cancel any fade out animations to prevent the element from fading out
      // and being removed. At this point, the tab's alertStates have changed
      // to a state in which this indicator should be visible.
      this.fadeOutAnimation_.cancel();
      this.fadeOutAnimation_ = null;
      this.fadeOutAnimationPromise_ = null;
    }

    if (this.fadeInAnimation_) {
      // If the element was already faded in, don't fade it in again
      return;
    }


    if (this.alertState_ === TabAlertState.kMediaRecording ||
        this.alertState_ === TabAlertState.kAudioRecording ||
        this.alertState_ === TabAlertState.kVideoRecording ||
        this.alertState_ === TabAlertState.kTabCapturing ||
        this.alertState_ === TabAlertState.kDesktopCapturing) {
      // Fade in and out 2 times and then fade in
      const totalDuration = 2600;
      this.fadeInAnimation_ = this.animate(
          [
            {opacity: 0, maxWidth: 0, offset: 0},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 200 / totalDuration},
            {opacity: 0, maxWidth: MAX_WIDTH, offset: 1200 / totalDuration},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 1400 / totalDuration},
            {opacity: 0, maxWidth: MAX_WIDTH, offset: 2400 / totalDuration},
            {opacity: 1, maxWidth: MAX_WIDTH, offset: 1},
          ],
          {
            duration: totalDuration,
            easing: 'linear',
            fill: 'forwards',
          });
    } else {
      this.fadeInAnimation_ = this.animate(
          [
            {opacity: 0, maxWidth: 0},
            {opacity: 1, maxWidth: MAX_WIDTH},
          ],
          {
            duration: this.fadeDurationMs_,
            fill: 'forwards',
          });
    }
  }

  hide(): Promise<void> {
    if (this.fadeInAnimation_) {
      // Cancel any fade in animations to prevent the element from fading in. At
      // this point, the tab's alertStates have changed to a state in which this
      // indicator should not be visible.
      this.fadeInAnimation_.cancel();
      this.fadeInAnimation_ = null;
    }

    if (this.fadeOutAnimationPromise_) {
      return this.fadeOutAnimationPromise_;
    }

    this.fadeOutAnimationPromise_ = new Promise((resolve, reject) => {
      this.fadeOutAnimation_ = this.animate(
          [
            {opacity: 1, maxWidth: MAX_WIDTH},
            {opacity: 0, maxWidth: 0},
          ],
          {
            duration: this.fadeDurationMs_,
            fill: 'forwards',
          });
      this.fadeOutAnimation_.addEventListener('cancel', () => {
        reject();
      });
      this.fadeOutAnimation_.addEventListener('finish', () => {
        this.remove();
        this.fadeOutAnimation_ = null;
        this.fadeOutAnimationPromise_ = null;
        resolve();
      });
    });

    return this.fadeOutAnimationPromise_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-alert-indicator': AlertIndicatorElement;
  }
}

customElements.define('tabstrip-alert-indicator', AlertIndicatorElement);

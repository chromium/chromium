// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './alert_indicator.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {AlertIndicatorElement} from './alert_indicator.js';
import {getTemplate} from './alert_indicators.html.js';
import {TabAlertState} from './tabs.mojom-webui.js';

export class AlertIndicatorsElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private containerEl_: HTMLElement;
  private alertIndicators_: Map<TabAlertState, AlertIndicatorElement>;

  constructor() {
    super();

    this.containerEl_ = this.getRequiredElement('#container');

    const audioIndicator = new AlertIndicatorElement();
    const recordingIndicator = new AlertIndicatorElement();

    this.alertIndicators_ = new Map([
      [TabAlertState.kMediaRecording, recordingIndicator],
      [TabAlertState.kAudioRecording, recordingIndicator],
      [TabAlertState.kVideoRecording, recordingIndicator],
      [TabAlertState.kTabCapturing, new AlertIndicatorElement()],
      [TabAlertState.kAudioPlaying, audioIndicator],
      [TabAlertState.kAudioMuting, audioIndicator],
      [TabAlertState.kBluetoothConnected, new AlertIndicatorElement()],
      [TabAlertState.kUsbConnected, new AlertIndicatorElement()],
      [TabAlertState.kHidConnected, new AlertIndicatorElement()],
      [TabAlertState.kSerialConnected, new AlertIndicatorElement()],
      [TabAlertState.kPipPlaying, new AlertIndicatorElement()],
      [TabAlertState.kDesktopCapturing, recordingIndicator],
      [TabAlertState.kVrPresentingInHeadset, new AlertIndicatorElement()],
    ]);
  }

  /**
   * @return A promise that resolves with the number of AlertIndicatorElements
   *     that are currently visible.
   */
  updateAlertStates(alertStates: TabAlertState[]): Promise<number> {
    const alertIndicators =
        alertStates.map(alertState => this.alertIndicators_.get(alertState));

    let alertIndicatorCount = 0;
    for (const [index, alertState] of alertStates.entries()) {
      const alertIndicator = alertIndicators[index];

      // Don't show unsupported indicators.
      if (!alertIndicator) {
        continue;
      }

      // If the same indicator appears earlier in the list of alert indicators,
      // that indicates that there is a higher priority alert state that
      // should display for the shared indicator.
      if (alertIndicators.indexOf(alertIndicator) < index) {
        continue;
      }

      // Always update alert state to ensure the correct icon is displayed.
      alertIndicator.alertState = alertState;

      this.containerEl_.insertBefore(
          alertIndicator,
          this.containerEl_.children[alertIndicatorCount] as Node);
      // Only fade in if this is just being added to the DOM.
      alertIndicator.show();

      alertIndicatorCount++;
    }

    const animationPromises = Array
                                  .from(
                                      this.containerEl_.children as
                                      HTMLCollectionOf<AlertIndicatorElement>)
                                  .slice(alertIndicatorCount)
                                  .map(indicator => indicator.hide());
    return Promise.all(animationPromises)
        .then(
            () => {
              return this.containerEl_.childElementCount;
            },
            () => {
              // A failure in the animation promises means an animation was
              // canceled and therefore there is a new set of alertStates
              // being animated.
              return -1;
            });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-alert-indicators': AlertIndicatorsElement;
  }
}

customElements.define('tabstrip-alert-indicators', AlertIndicatorsElement);

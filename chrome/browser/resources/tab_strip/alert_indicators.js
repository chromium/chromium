// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AlertIndicatorElement} from './alert_indicator.js';
import {CustomElement} from './custom_element.js';
import {TabAlertState} from './tabs_api_proxy.js';

export class AlertIndicatorsElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!HTMLElement} */
    this.containerEl_ = /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#container'));

    const audioIndicator = new AlertIndicatorElement();
    const recordingIndicator = new AlertIndicatorElement();

    /** @private {!Map<!TabAlertState, !AlertIndicatorElement>} */
    this.alertIndicators_ = new Map([
      [TabAlertState.MEDIA_RECORDING, recordingIndicator],
      [TabAlertState.TAB_CAPTURING, new AlertIndicatorElement()],
      [TabAlertState.AUDIO_PLAYING, audioIndicator],
      [TabAlertState.AUDIO_MUTING, audioIndicator],
      [TabAlertState.BLUETOOTH_CONNECTED, new AlertIndicatorElement()],
      [TabAlertState.USB_CONNECTED, new AlertIndicatorElement()],
      [TabAlertState.SERIAL_CONNECTED, new AlertIndicatorElement()],
      [TabAlertState.PIP_PLAYING, new AlertIndicatorElement()],
      [TabAlertState.DESKTOP_CAPTURING, recordingIndicator],
      [TabAlertState.VR_PRESENTING_IN_HEADSET, new AlertIndicatorElement()],
    ]);
  }

  /**
   * @param {!Array<!TabAlertState>} alertStates
   * @return {!Promise<number>} A promise that resolves with the number of
   *     AlertIndicatorElements that are currently visible.
   */
  updateAlertStates(alertStates) {
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
          alertIndicator, this.containerEl_.children[alertIndicatorCount]);
      // Only fade in if this is just being added to the DOM.
      alertIndicator.show();

      alertIndicatorCount++;
    }

    const animationPromises = Array.from(this.containerEl_.children)
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
            });
  }
}

customElements.define('tabstrip-alert-indicators', AlertIndicatorsElement);

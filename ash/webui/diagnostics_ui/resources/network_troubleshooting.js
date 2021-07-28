// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NetworkTroubleshootingElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NetworkTroubleshootingElement extends
    NetworkTroubleshootingElementBase {
  static get is() {
    return 'network-troubleshooting';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      networkType: {type: String, value: ''},
    }
  }

  /** @protected */
  onTroubleConnectingClicked_() {
    // TODO(michaelcheco): Add correct URL.
    window.open('https://support.google.com/chromebook?p=diagnostics_');
  }

  /**
   * @protected
   * @return {string}
   */
  computeTroubleConnectingText_() {
    return loadTimeData.getStringF('troubleshootingText', this.networkType);
  }
}

customElements.define(
    NetworkTroubleshootingElement.is, NetworkTroubleshootingElement);

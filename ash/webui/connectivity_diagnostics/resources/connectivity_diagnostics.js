// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics.js';
import 'chrome://resources/cr_components/chromeos/network_health/network_health_summary.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * Polymer element connectivity diagnostics UI.
 */

Polymer({
  is: 'connectivity-diagnostics',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, CrContainerShadowBehavior],

  /**
   * Boolean flag to show the feedback button in the app.
   * @private
   * @type {boolean}
   */
  showFeedbackBtn_: false,

  /** @override */
  attached() {
    this.getShowFeedbackBtn_();
    this.runAllRoutines_();
  },

  /**
   * Returns and typecasts the network diagnostics element
   * @returns {!NetworkDiagnosticsElement}
   * @private
   */
  getNetworkDiagnosticsElement_() {
    return /** @type {!NetworkDiagnosticsElement} */ (
        this.$$('#network-diagnostics'));
  },

  /** @private */
  runAllRoutines_() {
    this.getNetworkDiagnosticsElement_().runAllRoutines();
  },

  /** @private */
  onCloseClick_() {
    self.close();
  },

  /** @private */
  onRunAllRoutinesClick_() {
    this.runAllRoutines_();
  },

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   * @private
   */
  onSendFeedbackClick_() {
    chrome.send('sendFeedbackReport');
  },

  /** @private */
  getShowFeedbackBtn_() {
    sendWithPromise('getShowFeedbackButton').then(result => {
      this.set('showFeedbackBtn_', result[0]);
    });
  },
});

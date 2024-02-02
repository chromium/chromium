// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network_health/network_diagnostics.js';
import 'chrome://resources/ash/common/network_health/network_health_summary.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import {NetworkDiagnosticsElement} from 'chrome://resources/ash/common/network_health/network_diagnostics.js';
import {CrContainerShadowMixin} from 'chrome://resources/ash/common/cr_elements/cr_container_shadow_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './connectivity_diagnostics.html.js';

/**
 * @fileoverview
 * Polymer element connectivity diagnostics UI.
 */

export interface ConnectivityDiagnosticsElement {
  $: {
    networkDiagnostics: NetworkDiagnosticsElement,
  };
}

const ConnectivityDiagnosticsElementBase =
    I18nMixin(CrContainerShadowMixin(PolymerElement));

export class ConnectivityDiagnosticsElement extends
    ConnectivityDiagnosticsElementBase {
  static get is() {
    return 'connectivity-diagnostics';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Boolean flag to show the feedback button in the app.
       */
      showFeedbackBtn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private showFeedbackBtn_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.getShowFeedbackBtn_();
    this.runAllRoutines_();
  }

  private runAllRoutines_(): void {
    this.$.networkDiagnostics.runAllRoutines();
  }

  private onCloseClick_(): void {
    self.close();
  }

  private onRunAllRoutinesClick_(): void {
    this.runAllRoutines_();
  }

  /**
   * Handles requests to open the feedback report dialog. The provided string
   * in the event will be sent as a part of the feedback report.
   */
  private onSendFeedbackClick_(): void {
    chrome.send('sendFeedbackReport');
  }

  private getShowFeedbackBtn_(): void {
    sendWithPromise('getShowFeedbackButton').then(result => {
      this.showFeedbackBtn_ = result[0];
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'connectivity-diagnostics': ConnectivityDiagnosticsElement;
  }
}

customElements.define(
    ConnectivityDiagnosticsElement.is, ConnectivityDiagnosticsElement);

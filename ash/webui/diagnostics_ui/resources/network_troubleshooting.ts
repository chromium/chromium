// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TroubleshootingInfo} from './diagnostics_types.js';
import {getTemplate} from './network_troubleshooting.html.js';

const NetworkTroubleshootingElementBase = I18nMixin(PolymerElement);

export class NetworkTroubleshootingElement extends
    NetworkTroubleshootingElementBase {
  static get is(): string {
    return 'network-troubleshooting';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      isLoggedIn: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      troubleshootingInfo: {
        type: Object,
      },
    };
  }

  troubleshootingInfo: TroubleshootingInfo;
  protected isLoggedIn: boolean;

  protected onLinkTextClicked(): void {
    window.open(this.troubleshootingInfo.url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'network-troubleshooting': NetworkTroubleshootingElement;
  }
}

customElements.define(
    NetworkTroubleshootingElement.is, NetworkTroubleshootingElement);

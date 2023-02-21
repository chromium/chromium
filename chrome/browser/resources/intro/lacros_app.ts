// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {IntroBrowserProxy, IntroBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './lacros_app.html.js';

export class LacrosIntroAppElement extends PolymerElement {
  static get is() {
    return 'intro-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** URL for the profile picture */
      pictureUrl: {
        type: String,
        value: loadTimeData.getString('pictureUrl'),
      },

      /** Whether to show the detailed info about enterprise management */
      showEnterpriseInfo: {
        type: Boolean,
        value: loadTimeData.getString('enterpriseInfo').length > 0,
      },

      disableProceedButton_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private disableProceedButton_: boolean;
  private browserProxy_: IntroBrowserProxy =
      IntroBrowserProxyImpl.getInstance();

  /** Called when the proceed button is clicked. */
  private onProceed_() {
    this.disableProceedButton_ = true;
    this.browserProxy_.continueWithAccount();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'intro-app': LacrosIntroAppElement;
  }
}

customElements.define(LacrosIntroAppElement.is, LacrosIntroAppElement);

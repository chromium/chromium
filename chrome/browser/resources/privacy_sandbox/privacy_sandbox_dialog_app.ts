// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy} from './privacy_sandbox_dialog_browser_proxy.js';

const PrivacySandboxDialogAppElementBase = PolymerElement;

export class PrivacySandboxDialogAppElement extends
    PrivacySandboxDialogAppElementBase {
  static get is() {
    return 'privacy-sandbox-dialog-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      expanded_: Boolean,
      isConsent_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isConsent');
        },
      },
    };
  }

  private expanded_: boolean;
  private isConsent_: boolean;

  private onClose_() {
    PrivacySandboxDialogBrowserProxy.getInstance().closeDialog();
  }
}

customElements.define(
    PrivacySandboxDialogAppElement.is, PrivacySandboxDialogAppElement);

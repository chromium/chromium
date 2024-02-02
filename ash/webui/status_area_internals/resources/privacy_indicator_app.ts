// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {pageHandler} from './page_handler.js';
import {getTemplate} from './privacy_indicator_app.html.js';

/**
 * @fileoverview
 * 'privacy-indicator-app' defines the UI for the privacy indicator app
 * in the status area test page.
 */

export class PrivacyIndicatorAppElement extends PolymerElement {
  static get is() {
    return 'privacy-indicator-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      id: {
        type: String,
        value: '',
      },
      name: {
        type: String,
        value: '',
      },
      useCamera: {
        type: Boolean,
        value: false,
      },
      useMicrophone: {
        type: Boolean,
        value: false,
      },
    };
  }

  private appid: string = '';
  private name: string = '';
  private useCamera: boolean = false;
  private useMicrophone: boolean = false;

  onTriggerPrivacyIndicators(e: Event) {
    e.stopPropagation();

    if (!this.appid || !this.name) {
      return;
    }

    pageHandler.triggerPrivacyIndicators(
        this.appid, this.name, this.useCamera, this.useMicrophone);
  }
}

customElements.define(
    PrivacyIndicatorAppElement.is, PrivacyIndicatorAppElement);

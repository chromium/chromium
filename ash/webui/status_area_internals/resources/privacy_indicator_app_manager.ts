// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './privacy_indicator_app.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_indicator_app_manager.html.js';

const testAppIdPrefix = 'chromeos-status-area-test-app';

/**
 * @fileoverview
 * 'privacy-indicator-app-manager' defines the UI for the Privacy
 * Indicators app management section of the test page.
 */

export class PrivacyIndicatorAppManagerElement extends PolymerElement {
  static get is() {
    return 'privacy-indicator-app-manager';
  }

  static get template() {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      idLatest: {
        type: Number,
        value: 0,
      },
      appList: {
        type: Array,
        value: [],
      },
    };
  }

  private idLatest: number = 0;
  private appList: string[] = [];

  onAddPrivacyIndicatorsApp(e: Event) {
    e.stopPropagation();

    this.appList.push(testAppIdPrefix + this.idLatest);
    this.idLatest++;

    // Force `appList` to update in the UI.
    this.appList = this.appList.slice();
  }
}

customElements.define(
    PrivacyIndicatorAppManagerElement.is, PrivacyIndicatorAppManagerElement);

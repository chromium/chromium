// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';


/**
 * Simple container with a notice inside.
 * Shown when API keys are missing.
 * @polymer
 */
class ApiKeysNoticeElement extends PolymerElement {
  static get is() {
    return 'api-keys-notice-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      noticeContent: {
        value: '',
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.updateLocaleAndMaybeShowNotice();
  }

  updateLocaleAndMaybeShowNotice() {
    const missingApiId = 'missingAPIKeysNotice';
    if (!loadTimeData.valueExists(missingApiId)) {
      return;
    }

    this.noticeContent = loadTimeData.getValue(missingApiId);
    this.hidden = false;
  }
}

customElements.define(ApiKeysNoticeElement.is, ApiKeysNoticeElement);

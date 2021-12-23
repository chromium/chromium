// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The personalization-main component displays the main content of
 * the personalization hub.
 */

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationMain extends WithPersonalizationStore {
  static get is() {
    return 'personalization-main';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }
}

customElements.define(PersonalizationMain.is, PersonalizationMain);

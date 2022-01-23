// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './account_manager_shared_css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class ArcAccountPickerAppElement extends PolymerElement {
  static get is() {
    return 'arc-account-picker-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  ready() {
    super.ready();

    this.shadowRoot.querySelector('#osSettingsLink')
        .addEventListener(
            'click',
            () => this.dispatchEvent(new CustomEvent('opened-new-window')));
  }
}

customElements.define(
    ArcAccountPickerAppElement.is, ArcAccountPickerAppElement);

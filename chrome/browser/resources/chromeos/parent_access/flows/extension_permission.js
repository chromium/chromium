// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ExtensionPermission extends PolymerElement {
  static get is() {
    return 'extension-permission';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      permission: {type: String},
      detail: {type: String},
    };
  }

  showDetails(e) {
    this.setDetailsVisibility(true);
  }

  hideDetails(e) {
    this.setDetailsVisibility(false);
  }

  setDetailsVisibility(isVisible) {
    this.shadowRoot.querySelector('#details').hidden = !isVisible;
    this.shadowRoot.querySelector('#hide-details').hidden = !isVisible;
    this.shadowRoot.querySelector('#show-details').hidden = isVisible;
    if (isVisible) {
      this.shadowRoot.querySelector('#details').focus();
    } else {
      this.shadowRoot.querySelector('#show-details').focus();
    }
  }
}

customElements.define(ExtensionPermission.is, ExtensionPermission);

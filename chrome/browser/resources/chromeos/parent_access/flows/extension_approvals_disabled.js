// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ExtensionApprovalsDisabled extends PolymerElement {
  static get is() {
    return 'extension-approvals-disabled';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    ExtensionApprovalsDisabled.is, ExtensionApprovalsDisabled);

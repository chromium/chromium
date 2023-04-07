// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_approvals_template.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ExtensionApprovalsBefore extends PolymerElement {
  static get is() {
    return 'extension-approvals-before';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ExtensionApprovalsBefore.is, ExtensionApprovalsBefore);

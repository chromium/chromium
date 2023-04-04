// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16, getBase64EncodedSrcForPng} from '../utils.js';

export class ExtensionApprovalsBefore extends PolymerElement {
  static get is() {
    return 'extension-approvals-before';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      extensionIconSrc: {type: String},
      extensionName: {type: String},
    };
  }

  constructor() {
    super();
    /**
     * The extension icon, represented as a Base64 encoded
     * string.
     * @protected {string}
     */
    this.extensionIconSrc = '';
    /**
     * Display name of the extension.
     * @protected {string}
     */
    this.extensionName = '';
  }

  /** @override */
  ready() {
    super.ready();
    this.configureWithParams_();
  }

  /** @private */
  async configureWithParams_() {
    const response = await getParentAccessParams();
    const params = response.params.flowTypeParams.extensionApprovalsParams;
    if (params) {
      this.renderDetails_(params);
    } else {
      console.error('Failed to fetch extension approvals params.');
    }
  }

  /** @private */
  renderDetails_(params) {
    this.extensionIconSrc = getBase64EncodedSrcForPng(params.iconPngBytes);
    this.extensionName = decodeMojoString16(params.extensionName);
  }
}

customElements.define(ExtensionApprovalsBefore.is, ExtensionApprovalsBefore);

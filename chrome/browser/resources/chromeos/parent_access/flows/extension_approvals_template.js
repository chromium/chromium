// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_permission.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessEvent} from '../parent_access_app.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16, getBase64EncodedSrcForPng} from '../utils.js';

export class ExtensionApprovalsTemplate extends PolymerElement {
  static get is() {
    return 'extension-approvals-template';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      screenTitle: {type: String},
      screenSubtitle: {type: String},
      extensionIconSrc: {type: String},
      extensionName: {type: String},
      extensionPermissions: {type: Array},
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
    /**
     * Localized permission strings.
     * @protected {Array<Object>}
     */
    this.extensionPermissions = [];
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
      this.dispatchEvent(new CustomEvent(ParentAccessEvent.SHOW_ERROR, {
        bubbles: true,
        composed: true,
      }));
    }
  }


  /** @private */
  renderDetails_(params) {
    this.extensionIconSrc = getBase64EncodedSrcForPng(params.iconPngBytes);
    this.extensionName = decodeMojoString16(params.extensionName);
    this.extensionPermissions = params.permissions.map((permission) => {
      return {
        permission: decodeMojoString16(permission.permission),
        details: decodeMojoString16(permission.details),
      };
    });
  }
}

customElements.define(
    ExtensionApprovalsTemplate.is, ExtensionApprovalsTemplate);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionApprovalsParams_ExtensionApprovalType} from '../parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16, getBase64EncodedSrcForPng} from '../utils.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ExtensionApprovalsBeforeBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

export class ExtensionApprovalsBefore extends ExtensionApprovalsBeforeBase {
  static get is() {
    return 'extension-approvals-before';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      title: {type: String},
      subtitle: {type: String},
      extensionIconSrc: {type: String},
      extensionName: {type: String},
    };
  }

  constructor() {
    super();
    /**
     * Title of the screen shown to the user.
     * @protected {string}
     */
    this.title = '';
    /**
     * Subtitle of the screen shown to the user.
     * @protected {string}
     */
    this.subtitle = '';
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

    switch (params.approvalType) {
      case ExtensionApprovalsParams_ExtensionApprovalType.kAdd:
        this.title = this.i18n('extensionApprovalsAddExtensionBeforeTitle');
        this.subtitle =
            this.i18n('extensionApprovalsAddExtensionBeforeSubtitle');
        break;
      case ExtensionApprovalsParams_ExtensionApprovalType.kEnable:
        this.title = this.i18n('extensionApprovalsEnableExtensionBeforeTitle');
        this.subtitle =
            this.i18n('extensionApprovalsEnableExtensionBeforeSubtitle');
        break;
    }
  }
}

customElements.define(ExtensionApprovalsBefore.is, ExtensionApprovalsBefore);

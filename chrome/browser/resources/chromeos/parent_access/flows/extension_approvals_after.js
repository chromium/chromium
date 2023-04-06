// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_approvals_template.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16} from '../utils.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ExtensionApprovalsAfterBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

export class ExtensionApprovalsAfter extends ExtensionApprovalsAfterBase {
  static get is() {
    return 'extension-approvals-after';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      childDisplayName: {type: String},
    };
  }

  constructor() {
    super();
    /**
     * Title of the screen shown to the user.
     * @protected {string}
     */
    this.childDisplayName = '';
  }

  /** @override */
  ready() {
    super.ready();
    this.getChildDisplayName_();
  }

  /** @private */
  async getChildDisplayName_() {
    const response = await getParentAccessParams();
    const params = response.params.flowTypeParams.extensionApprovalsParams;
    if (params) {
      this.childDisplayName = decodeMojoString16(params.childDisplayName);
    } else {
      this.dispatchEvent(new CustomEvent('show-error', {
        bubbles: true,
        composed: true,
      }));
    }
  }
}

customElements.define(ExtensionApprovalsAfter.is, ExtensionApprovalsAfter);

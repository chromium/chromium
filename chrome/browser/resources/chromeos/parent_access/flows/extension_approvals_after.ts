// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension_approvals_template.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessEvent} from '../parent_access_app.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16} from '../utils.js';

import {getTemplate} from './extension_approvals_after.html.js';

const ExtensionApprovalsAfterBase = I18nMixin(PolymerElement);

export class ExtensionApprovalsAfter extends ExtensionApprovalsAfterBase {
  static get is() {
    return 'extension-approvals-after';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      childDisplayName: {type: String},
    };
  }

  childDisplayName: string;

  override ready() {
    super.ready();
    this.getChildDisplayName();
  }

  private async getChildDisplayName() {
    const response = await getParentAccessParams();
    const params = response!.params.flowTypeParams!.extensionApprovalsParams;
    if (params) {
      this.childDisplayName = decodeMojoString16(params.childDisplayName);
    } else {
      this.dispatchEvent(new CustomEvent(ParentAccessEvent.SHOW_ERROR, {
        bubbles: true,
        composed: true,
      }));
    }
  }
}

customElements.define(ExtensionApprovalsAfter.is, ExtensionApprovalsAfter);

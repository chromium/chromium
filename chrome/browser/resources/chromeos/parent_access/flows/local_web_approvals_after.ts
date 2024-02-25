// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WebApprovalsParams} from '../parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16, getBase64EncodedSrcForPng} from '../utils.js';

import {getTemplate} from './local_web_approvals_after.html.js';

const LocalWebApprovalsAfterBase = I18nMixin(PolymerElement);

export class LocalWebApprovalsAfter extends LocalWebApprovalsAfterBase {
  static get is() {
    return 'local-web-approvals-after';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      childName: {type: String},
      url: {type: String},
      favicon: {type: String},
    };
  }

  childName: string;
  url: string;
  favicon: string;

  override ready() {
    super.ready();
    this.configureWithParams();
  }

  private async configureWithParams() {
    const response = await getParentAccessParams();
    const params = response!.params.flowTypeParams!.webApprovalsParams;
    if (params) {
      this.renderDetails(params);
    } else {
      console.error('Failed to fetch web approvals params.');
    }
  }

  /**
   * Renders local approvals specific information from the WebApprovalsParams.
   */
  private renderDetails(params: WebApprovalsParams) {
    this.childName = decodeMojoString16(params.childDisplayName);
    this.url = params.url.url;
    this.favicon = getBase64EncodedSrcForPng(params.faviconPngBytes);
  }
}

customElements.define(
    LocalWebApprovalsAfter.is, LocalWebApprovalsAfter);

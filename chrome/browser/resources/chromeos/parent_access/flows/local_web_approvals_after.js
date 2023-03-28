// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WebApprovalsParams} from '../parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';
import {decodeMojoString16, getBase64EncodedSrcForPng} from '../utils.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const LocalWebApprovalsAfterBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

export class LocalWebApprovalsAfterElement extends LocalWebApprovalsAfterBase {
  static get is() {
    return 'local-web-approvals-after';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      childName: {type: String},
      url: {type: String},
      favicon: {type: String},
    };
  }

  constructor() {
    super();

    /**
     * Display name for the supervised user requesting website access.
     * @protected {string}
     */
    this.childName = '';
    /**
     * URL that access is being requested for.
     * @protected {string}
     */
    this.url = '';
    /**
     * Favicon for the website, encoded as a Base64 encoded string.
     * @protected {string}
     */
    this.favicon = '';
  }

  /** @override */
  ready() {
    super.ready();
    this.configureWithParams_();
  }

  /** @private */
  async configureWithParams_() {
    const response = await getParentAccessParams();
    const params = response.params.flowTypeParams.webApprovalsParams;
    if (params) {
      this.renderDetails_(params);
    } else {
      console.error('Failed to fetch web approvals params.');
    }
  }

  /**
   * Renders local approvals specific information from the WebApprovalsParams
   * @param {!WebApprovalsParams} params
   * @private
   */
  renderDetails_(params) {
    this.childName = decodeMojoString16(params.childDisplayName);
    this.url = params.url.url;
    this.favicon = getBase64EncodedSrcForPng(params.faviconPngBytes);
  }
}

customElements.define(
    LocalWebApprovalsAfterElement.is, LocalWebApprovalsAfterElement);

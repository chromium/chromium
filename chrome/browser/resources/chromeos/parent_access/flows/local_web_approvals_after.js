// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WebApprovalsParams} from '../parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from '../parent_access_ui_handler.js';

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
    const childName = this.decodeMojoString16_(params.childDisplayName);
    const url = params.url.url;
    // Convert the PNG bytes to a Base64 encoded string.
    const favicon = btoa(String.fromCharCode(...params.faviconPngBytes));

    this.shadowRoot.querySelector('.subtitle').innerText =
        this.i18n('localWebApprovalsAfterSubtitle', childName);
    this.shadowRoot.querySelector('.details-text').innerText =
        this.i18n('localWebApprovalsAfterDetails', url);
    this.shadowRoot.querySelector('.favicon').src =
        'data:image/png;base64,' + favicon;
  }

  /**
   * @param {!String16} str
   * @private
   */
  decodeMojoString16_(str) {
    return str.data.map((ch) => String.fromCodePoint(ch)).join('');
  }
}

customElements.define(
    LocalWebApprovalsAfterElement.is, LocalWebApprovalsAfterElement);

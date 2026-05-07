// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './accessibility_annotator_info.css.js';
import {getHtml} from './accessibility_annotator_info.html.js';
import {AccessibilityAnnotatorInfoBrowserProxy} from './browser_proxy.js';

export class AccessibilityAnnotatorInfoElement extends CrLitElement {
  static get is() {
    return 'accessibility-annotator-info';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      email_: {type: String},
      avatarUrl_: {type: String},
    };
  }

  protected accessor email_: string = '';
  protected accessor avatarUrl_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    AccessibilityAnnotatorInfoBrowserProxy.getInstance()
        .handler.getAccountInfo()
        .then(response => {
          if (response.info) {
            this.email_ = response.info.email;
            this.avatarUrl_ = response.info.avatarUrl;
          }
          AccessibilityAnnotatorInfoBrowserProxy.getInstance().handler.showUi();
        });
  }

  protected onManageSettingsClick_() {
    AccessibilityAnnotatorInfoBrowserProxy.getInstance()
        .handler.onManageSettingsClicked();
  }

  protected onGotItClick_() {
    AccessibilityAnnotatorInfoBrowserProxy.getInstance()
        .handler.onInfoAcknowledged();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accessibility-annotator-info': AccessibilityAnnotatorInfoElement;
  }
}

customElements.define(
    AccessibilityAnnotatorInfoElement.is, AccessibilityAnnotatorInfoElement);

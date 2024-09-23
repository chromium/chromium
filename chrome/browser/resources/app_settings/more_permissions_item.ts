// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './more_permissions_item.css.js';
import {getHtml} from './more_permissions_item.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class MorePermissionsItemElement extends CrLitElement {
  static get is() {
    return 'app-management-more-permissions-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      morePermissionsLabel: {type: String},
    };
  }

  app: App = createDummyApp();
  morePermissionsLabel: string = '';

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
  }

  private onClick_() {
    BrowserProxy.getInstance().handler.openNativeSettings(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NATIVE_SETTINGS_OPENED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-more-permissions-item': MorePermissionsItemElement;
  }
}

customElements.define(
    MorePermissionsItemElement.is, MorePermissionsItemElement);

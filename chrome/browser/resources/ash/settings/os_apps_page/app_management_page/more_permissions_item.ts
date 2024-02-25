// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_cros_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './more_permissions_item.html.js';

export class AppManagementMorePermissionsItemElement extends PolymerElement {
  static get is() {
    return 'app-management-more-permissions-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,
      morePermissionsLabel: String,
    };
  }

  app: App;
  morePermissionsLabel: string;

  override ready(): void {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  private onClick_(): void {
    BrowserProxy.getInstance().handler.openNativeSettings(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NATIVE_SETTINGS_OPENED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-more-permissions-item':
        AppManagementMorePermissionsItemElement;
  }
}

customElements.define(
    AppManagementMorePermissionsItemElement.is,
    AppManagementMorePermissionsItemElement);

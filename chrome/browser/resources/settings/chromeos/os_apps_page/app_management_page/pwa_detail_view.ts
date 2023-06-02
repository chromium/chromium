// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_details_item.js';
import './pin_to_shelf_item.js';
import './supported_links_item.js';
import './sub_apps_item.js';
import './app_management_cros_shared_style.css.js';
import 'chrome://resources/cr_components/app_management/file_handling_item.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pwa_detail_view.html.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementPwaDetailViewElementBase =
    AppManagementStoreMixin(PolymerElement);

export class AppManagementPwaDetailViewElement extends
    AppManagementPwaDetailViewElementBase {
  static get is() {
    return 'app-management-pwa-detail-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app_: Object,
    };
  }

  private app_: App;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-pwa-detail-view': AppManagementPwaDetailViewElement;
  }
}

customElements.define(
    AppManagementPwaDetailViewElement.is, AppManagementPwaDetailViewElement);

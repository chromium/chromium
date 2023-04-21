// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_details_item.js';
import './pin_to_shelf_item.js';
import './resize_lock_item.js';
import './supported_links_item.js';
import './app_management_cros_shared_style.css.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getPermission, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './arc_detail_view.html.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementArcDetailViewElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

class AppManagementArcDetailViewElement extends
    AppManagementArcDetailViewElementBase {
  static get is() {
    return 'app-management-arc-detail-view';
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

  /**
   * Returns true if the app has not requested any permissions.
   */
  private noPermissionsRequested_(app: App): boolean {
    const permissionItems =
        this.shadowRoot!.querySelector('#subpermissionList')!.querySelectorAll(
            'app-management-permission-item');
    for (let i = 0; i < permissionItems.length; i++) {
      const permissionItem = permissionItems[i];
      const permission = getPermission(app, permissionItem.permissionType);
      if (permission !== undefined) {
        return false;
      }
    }
    return true;
  }

  private getMorePermissionsLabel_(): string {
    return loadTimeData.getBoolean('appManagementArcReadOnlyPermissions') ?
        this.i18n('appManagementArcManagePermissionsLabel') :
        this.i18n('appManagementMorePermissionsLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-arc-detail-view': AppManagementArcDetailViewElement;
  }
}

customElements.define(
    AppManagementArcDetailViewElement.is, AppManagementArcDetailViewElement);

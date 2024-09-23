// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_details_item.js';
import './permission_heading.js';
import './pin_to_shelf_item.js';
import './sub_apps_item.js';
import './app_management_cros_shared_style.css.js';
import './file_handling_item.js';
import '../../app_management_icons.html.js';
import './more_permissions_item.js';
import './permission_item.js';
import './supported_links_item.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';
import {isRevampWayfindingEnabled} from '../../common/load_time_booleans.js';
import {PrefsState} from '../../common/types.js';

import {getTemplate} from './pwa_detail_view.html.js';

const AppManagementPwaDetailViewElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

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
      prefs: {
        type: Object,
        notify: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      app_: Object,
      apps_: Object,
    };
  }

  prefs: PrefsState;
  private app_: App;
  private apps_: AppMap;
  private isRevampWayfindingEnabled_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  }

  private getAppManagementMorePermissionsLabel_(): string {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('appManagementMorePermissionsLabelWebApp') :
        this.i18n('appManagementMorePermissionsLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-pwa-detail-view': AppManagementPwaDetailViewElement;
  }
}

customElements.define(
    AppManagementPwaDetailViewElement.is, AppManagementPwaDetailViewElement);

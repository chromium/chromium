// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_details_item.js';
import './pin_to_shelf_item.js';
import './read_only_permission_item.js';
import './resize_lock_item.js';
import './supported_links_item.js';
import './app_management_cros_shared_style.css.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {getPermission, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './arc_detail_view.html.js';
import {AppManagementStoreMixin} from './store_mixin.js';

const AppManagementArcDetailViewElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

interface PermissionDefinition {
  /** The type of the permission, e.g. kStorage. */
  type: PermissionTypeIndex;
  /**
   * The icon to use for the permission, as a name from an iron-icon iconset.
   */
  icon: string;
  /** The ID of a string resource to use as label for the permission. */
  labelId: string;
}

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

      /**
       * Static definition for the list of permissions to display for the app.
       */
      permissionDefinitions_: {
        type: Array,
        value: (): PermissionDefinition[] => {
          return [
            {
              type: 'kLocation',
              icon: 'app-management:location',
              labelId: 'appManagementLocationPermissionLabel',
            },
            {
              type: 'kCamera',
              icon: 'app-management:camera',
              labelId: 'appManagementCameraPermissionLabel',
            },
            {
              type: 'kMicrophone',
              icon: 'app-management:microphone',
              labelId: 'appManagementMicrophonePermissionLabel',
            },
            {
              type: 'kContacts',
              icon: 'app-management:contacts',
              labelId: 'appManagementContactsPermissionLabel',
            },
            {
              type: 'kStorage',
              icon: 'app-management:storage',
              labelId: 'appManagementStoragePermissionLabel',
            },
          ];
        },
      },

      hasReadOnlyPermissions_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('appManagementArcReadOnlyPermissions'),
        readOnly: true,
      },
    };
  }

  private app_: App;
  private permissionDefinitions_: PermissionDefinition[];
  private hasReadOnlyPermissions_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }

  /**
   * Returns true if the app has not requested any permissions.
   */
  private noPermissionsRequested_(
      app: App, permissionDefinitions: PermissionDefinition[]): boolean {
    if (app === undefined) {
      return true;
    }

    for (const permissionDef of permissionDefinitions) {
      const permission = getPermission(app, permissionDef.type);
      if (permission !== undefined) {
        return false;
      }
    }
    return true;
  }

  private getMorePermissionsLabel_(): string {
    return this.hasReadOnlyPermissions_ ?
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

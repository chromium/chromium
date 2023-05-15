// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {getPermission, getPermissionValueAsTriState} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './read_only_permission_item.html.js';

const AppManagementReadOnlyPermissionItemElementBase =
    I18nMixin(PolymerElement);

export class AppManagementReadOnlyPermissionItemElement extends
    AppManagementReadOnlyPermissionItemElementBase {
  static get is() {
    return 'app-management-read-only-permission-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The name of the permission, to be displayed to the user.
       */
      permissionLabel: String,

      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: {
        type: String,
        reflectToAttribute: true,
      },

      icon: String,

      app: Object,

      /**
       * True if the permission type is available for the app.
       */
      available_: {
        type: Boolean,
        computed: 'computeAvailable_(app, permissionType)',
        reflectToAttribute: true,
      },
    };
  }

  app: App;
  permissionLabel: string;
  permissionType: PermissionTypeIndex;
  icon: string;
  private available_: boolean;

  private computeAvailable_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    return getPermission(app, permissionType) !== undefined;
  }

  private getPermissionDescriptionString_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): string {
    if (app === undefined || permissionType === undefined) {
      return '';
    }

    const permission = getPermission(app, permissionType);
    assert(permission);

    const value = getPermissionValueAsTriState(app, permissionType);

    if (value === TriState.kAllow && permission.details) {
      return this.i18n(
          'appManagementPermissionAllowedWithDetails', permission.details);
    }

    switch (value) {
      case TriState.kAllow:
        return this.i18n('appManagementPermissionAllowed');
      case TriState.kBlock:
        return this.i18n('appManagementPermissionDenied');
      case TriState.kAsk:
        return this.i18n('appManagementPermissionAsk');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppManagementReadOnlyPermissionItemElement.is]:
        AppManagementReadOnlyPermissionItemElement;
  }
}

customElements.define(
    AppManagementReadOnlyPermissionItemElement.is,
    AppManagementReadOnlyPermissionItemElement);

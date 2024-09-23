// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {App, Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {InstallReason, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import type {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermission, createTriStatePermission, getBoolPermissionValue, getTriStatePermissionValue, isBoolValue, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermission, getPermissionValueBool, recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {CrLitElement, type PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './permission_item.css.js';
import {getHtml} from './permission_item.html.js';
import type {ToggleRowElement} from './toggle_row.js';
import {createDummyApp} from './web_app_settings_utils.js';

export class PermissionItemElement extends CrLitElement {
  static get is() {
    return 'app-management-permission-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The name of the permission, to be displayed to the user.
       */
      permissionLabel: {type: String},

      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: {
        type: String,
        reflect: true,
      },

      icon: {type: String},

      /**
       * If set to true, toggling the permission item will not set the
       * permission in the backend. Call `syncPermission()` to set the
       * permission to reflect the current UI state.
       */
      syncPermissionManually: {type: Boolean},

      app: {type: Object},

      /**
       * True if the permission type is available for the app.
       */
      available_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  app: App = createDummyApp();
  permissionLabel: string = '';
  permissionType: PermissionTypeIndex = 'kUnknown';
  icon: string = '';
  private syncPermissionManually: boolean = false;
  protected available_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app') ||
        changedProperties.has('permissionType')) {
      this.available_ = this.isAvailable_();
    }
  }

  override firstUpdated() {
    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.togglePermission_);
  }

  private isAvailable_(): boolean {
    return getPermission(this.app, this.permissionType) !== undefined;
  }

  protected isManaged_(): boolean {
    if (!this.isAvailable_()) {
      return false;
    }

    const permission = getPermission(this.app, this.permissionType);
    assert(permission);
    return permission.isManaged;
  }

  protected isDisabled_(): boolean {
    if (this.app.installReason === InstallReason.kSubApp) {
      return true;
    }

    return this.isManaged_();
  }

  protected getValue_(): boolean {
    return getPermissionValueBool(this.app, this.permissionType);
  }

  resetToggle() {
    const currentValue = this.getValue_();
    this.shadowRoot!.querySelector<ToggleRowElement>('#toggle-row')!.setToggle(
        currentValue);
  }

  private onClick_() {
    this.shadowRoot!.querySelector<ToggleRowElement>('#toggle-row')!.click();
  }

  private togglePermission_() {
    if (!this.syncPermissionManually) {
      this.syncPermission();
    }
  }

  /**
   * Set the permission to match the current UI state. This only needs to be
   * called when `syncPermissionManually` is set.
   */
  syncPermission() {
    let newPermission: Permission|undefined = undefined;

    let newBoolState = false;
    const permission = getPermission(this.app, this.permissionType);
    assert(permission);
    const permissionValue = permission.value;
    if (isBoolValue(permissionValue)) {
      newPermission =
          this.getUiPermissionBoolean_(this.app, this.permissionType);
      newBoolState = getBoolPermissionValue(newPermission.value);
    } else if (isTriStateValue(permissionValue)) {
      newPermission =
          this.getUiPermissionTriState_(this.app, this.permissionType);

      newBoolState =
          getTriStatePermissionValue(newPermission.value) === TriState.kAllow;
    } else {
      assertNotReached();
    }

    BrowserProxy.getInstance().handler.setPermission(
        this.app.id, newPermission!);

    recordAppManagementUserAction(
        this.app.type,
        this.getUserMetricActionForPermission_(
            newBoolState, this.permissionType));
  }

  /**
   * Gets the permission boolean based on the toggle's UI state.
   */
  private getUiPermissionBoolean_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    const currentPermission = getPermission(app, permissionType);
    assert(currentPermission);

    assert(isBoolValue(currentPermission.value));

    const newPermissionValue = !getBoolPermissionValue(currentPermission.value);

    return createBoolPermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  }

  /**
   * Gets the permission tristate based on the toggle's UI state.
   */
  private getUiPermissionTriState_(
      app: App, permissionType: PermissionTypeIndex): Permission {
    let newPermissionValue;
    const currentPermission = getPermission(app, permissionType);
    assert(currentPermission);

    assert(isTriStateValue(currentPermission.value));

    switch (getTriStatePermissionValue(currentPermission.value)) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        // TODO(rekanorman): Eventually TriState.kAsk, but currently changing a
        // permission to kAsk then opening the site settings page for the app
        // produces the error:
        // "Only extensions or enterprise policy can change the setting to ASK."
        newPermissionValue = TriState.kBlock;
        break;
      default:
        assertNotReached();
    }

    return createTriStatePermission(
        PermissionType[permissionType], newPermissionValue,
        currentPermission.isManaged);
  }

  private getUserMetricActionForPermission_(
      permissionValue: boolean,
      permissionType: PermissionTypeIndex): AppManagementUserAction {
    switch (permissionType) {
      case 'kNotifications':
        return permissionValue ?
            AppManagementUserAction.NOTIFICATIONS_TURNED_ON :
            AppManagementUserAction.NOTIFICATIONS_TURNED_OFF;

      case 'kLocation':
        return permissionValue ? AppManagementUserAction.LOCATION_TURNED_ON :
                                 AppManagementUserAction.LOCATION_TURNED_OFF;

      case 'kCamera':
        return permissionValue ? AppManagementUserAction.CAMERA_TURNED_ON :
                                 AppManagementUserAction.CAMERA_TURNED_OFF;

      case 'kMicrophone':
        return permissionValue ? AppManagementUserAction.MICROPHONE_TURNED_ON :
                                 AppManagementUserAction.MICROPHONE_TURNED_OFF;

      case 'kContacts':
        return permissionValue ? AppManagementUserAction.CONTACTS_TURNED_ON :
                                 AppManagementUserAction.CONTACTS_TURNED_OFF;

      case 'kStorage':
        return permissionValue ? AppManagementUserAction.STORAGE_TURNED_ON :
                                 AppManagementUserAction.STORAGE_TURNED_OFF;

      case 'kPrinting':
        return permissionValue ? AppManagementUserAction.PRINTING_TURNED_ON :
                                 AppManagementUserAction.PRINTING_TURNED_OFF;

      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-permission-item': PermissionItemElement;
  }
}

customElements.define(PermissionItemElement.is, PermissionItemElement);

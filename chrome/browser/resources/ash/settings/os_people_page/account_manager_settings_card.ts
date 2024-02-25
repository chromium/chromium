// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'account-manager-settings-card' is the card element containing the account
 * information.
 */

import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../assert_extras.js';
import {isChild} from '../common/load_time_booleans.js';
import {ParentalControlsBrowserProxyImpl} from '../parental_controls_page/parental_controls_browser_proxy.js';

import {Account} from './account_manager_browser_proxy.js';
import {getTemplate} from './account_manager_settings_card.html.js';

const AccountManagerSettingsCardElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class AccountManagerSettingsCardElement extends
    AccountManagerSettingsCardElementBase {
  static get is() {
    return 'account-manager-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Primary / Device account.
       */
      deviceAccount: Object,

      isChildUser_: {
        type: Boolean,
        value() {
          return isChild();
        },
        readOnly: true,
      },

      isDeviceAccountManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDeviceAccountManaged');
        },
        readOnly: true,
      },

      /**
       * True if secondary account sign-ins are allowed, false otherwise.
       */
      isSecondaryGoogleAccountSigninAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
        },
        readOnly: true,
      },

      /**
       * The name of the icon to display in the management row.
       * Should only be read if isDeviceAccountManaged_ is true.
       */
      managedByIcon_: {
        type: String,
        value() {
          return loadTimeData.getString('managedByIcon');
        },
      },
    };
  }

  deviceAccount: Account|null;
  private isChildUser_: boolean;
  private isDeviceAccountManaged_: boolean;
  private isSecondaryGoogleAccountSigninAllowed_: boolean;

  private onManagedIconClick_(): void {
    if (this.isChildUser_) {
      ParentalControlsBrowserProxyImpl.getInstance().launchFamilyLinkSettings();
    }
  }

  private getAccountManagerDescription_(): TrustedHTML {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return this.i18nAdvanced('accountManagerChildDescription');
    }
    return this.i18nAdvanced('accountManagerDescription');
  }

  private getManagementDescription_(): TrustedHTML|string {
    if (this.isChildUser_) {
      return this.i18nAdvanced('accountManagerManagementDescription');
    }
    if (!this.deviceAccount) {
      return '';
    }
    assertExists(this.deviceAccount.organization);
    if (!this.deviceAccount.organization) {
      if (this.isDeviceAccountManaged_) {
        console.error(
            'The device account is managed, but the organization is not set.');
      }
      return '';
    }
    // Format: 'This account is managed by
    //          <a target="_blank" href="chrome://management">google.com</a>'.
    // Where href will be set by <localized-link>.
    return this.i18nAdvanced('accountManagerManagementDescription', {
      substitutions: [
        this.deviceAccount.organization,
      ],
    });
  }

  /**
   * @return true if managed badge should be shown next to the device
   * account picture.
   */
  private shouldShowManagedBadge_(): boolean {
    return this.isDeviceAccountManaged_ && !this.isChildUser_;
  }

  /**
   * @return a CSS image-set for multiple scale factors.
   */
  private getIconImageSet_(): string {
    if (!this.deviceAccount) {
      return '';
    }
    return getImage(this.deviceAccount.pic);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AccountManagerSettingsCardElement.is]: AccountManagerSettingsCardElement;
  }
}

customElements.define(
    AccountManagerSettingsCardElement.is, AccountManagerSettingsCardElement);

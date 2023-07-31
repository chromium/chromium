// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SafetyHubBrowserProxyImpl, SafetyHubEvent, UnusedSitePermissions} from '../safety_hub/safety_hub_browser_proxy.js';

import {getTemplate} from './safety_hub_page.html.js';

const SettingsSafetyHubPageElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSafetyHubPageElement extends
    SettingsSafetyHubPageElementBase {
  static get is() {
    return 'settings-safety-hub-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The string for the header of Passsword Check card.
      passwordStateHeader_: String,

      // The string for the subheader of Password Check card.
      passwordStateSubheader_: String,

      // The string for the header of Version Check card.
      versionStateHeader_: String,

      // The string for the subheader of Version Check card.
      versionStateSubheader_: String,

      // The string for the header of Safe Browsing card.
      safeBrowsingHeader_: String,

      // The string for the subheader of Safe Browsing card.
      safeBrowsingSubheader_: String,

      // Whether Unused Site Permissions module should be visible.
      showUnusedSitePermissions_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private passwordStateHeader_: string;
  private passwordStateSubheader_: string;
  private versionStateHeader_: string;
  private versionStateSubheader_: string;
  private safeBrowsingHeader_: string;
  private safeBrowsingSubheader_: string;
  private showUnusedSitePermissions_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    // TODO(crbug.com/1443466): Replace strings with the real values.
    this.passwordStateHeader_ = this.i18n('privacyPageTitle');
    this.passwordStateSubheader_ = this.i18n('privacyPageTitle');
    this.versionStateHeader_ = this.i18n('privacyPageTitle');
    this.versionStateSubheader_ = this.i18n('privacyPageTitle');
    this.safeBrowsingHeader_ = this.i18n('privacyPageTitle');
    this.safeBrowsingSubheader_ = this.i18n('privacyPageTitle');

    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    SafetyHubBrowserProxyImpl.getInstance()
        .getRevokedUnusedSitePermissionsList()
        .then(
            (sites: UnusedSitePermissions[]) =>
                this.onUnusedSitePermissionListChanged_(sites));
  }

  private onUnusedSitePermissionListChanged_(permissions:
                                                 UnusedSitePermissions[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showUnusedSitePermissions_ =
        permissions.length > 0 || this.showUnusedSitePermissions_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);

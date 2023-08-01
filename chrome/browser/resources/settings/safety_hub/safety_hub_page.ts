// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './safety_hub_card.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {CardInfo, CardState, SafetyHubBrowserProxyImpl, SafetyHubEvent, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {getTemplate} from './safety_hub_page.html.js';

export interface SettingsSafetyHubPageElement {
  $: {
    passwords: HTMLElement,
    safeBrowsing: HTMLElement,
    version: HTMLElement,
  };
}

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
      // The object that holds data of Password Check card.
      passwordCardData_: Object,

      // The object that holds data of Version Check card.
      versionCardData_: Object,

      // The object that holds data of Safe Browsing card.
      safeBrowsingCardData_: Object,

      // Whether Unused Site Permissions module should be visible.
      showUnusedSitePermissions_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private passwordCardData_: CardInfo;
  private versionCardData_: CardInfo;
  private safeBrowsingCardData_: CardInfo;
  private showUnusedSitePermissions_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    this.initializeCards_();
    this.initializeModules_();
  }

  private initializeCards_() {
    // TODO(crbug.com/1443466): Replace dummy values with the real values.
    const dummyText = this.i18n('privacyPageTitle');
    this.passwordCardData_ = {
      header: dummyText,
      subheader: dummyText,
      state: CardState.INFO,
    };
    this.versionCardData_ = {
      header: dummyText,
      subheader: dummyText,
      state: CardState.INFO,
    };

    this.safeBrowsingCardData_ = {
      header: dummyText,
      subheader: dummyText,
      state: CardState.INFO,
    };
  }

  private initializeModules_() {
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

  private onPasswordsClick_() {
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.CHECKUP);
  }

  private onVersionClick_() {
    Router.getInstance().navigateTo(
        routes.ABOUT, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  private onSafeBrowsingClick_() {
    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
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

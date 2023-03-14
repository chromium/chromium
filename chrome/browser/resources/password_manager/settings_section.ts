// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import {getTemplate} from './settings_section.html.js';
import {SyncBrowserProxyImpl, TrustedVaultBannerState} from './sync_browser_proxy.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    blockedSitesList: HTMLElement,
    passwordToggle: PrefToggleButtonElement,
    trustedVaultBanner: CrLinkRowElement,
  };
}

const SettingsSectionElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSectionElement extends SettingsSectionElementBase {
  static get is() {
    return 'settings-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** An array of blocked sites to display. */
      blockedSites_: {
        type: Array,
        value: () => [],
      },

      isPasswordManagerShortcutInstalled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isPasswordManagerShortcutInstalled');
        },
      },

      // <if expr="is_win or is_macosx">
      isBiometricAuthenticationForFillingToggleVisible_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'biometricAuthenticationForFillingToggleVisible');
        },
      },
      // </if>

      hasPasswordsToExport_: {
        type: Boolean,
        value: false,
      },

      /** The visibility state of the trusted vault banner. */
      trustedVaultBannerState_: {
        type: Object,
        value: TrustedVaultBannerState.NOT_SHOWN,
      },
    };
  }

  private blockedSites_: BlockedSite[];

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;
  private setCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  private hasPasswordsToExport_: boolean;

  private trustedVaultBannerState_: TrustedVaultBannerState;

  override connectedCallback() {
    super.connectedCallback();
    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };
    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);

    this.setCredentialsChangedListener_ =
        (passwords: chrome.passwordsPrivate.PasswordUiEntry[]) => {
          this.hasPasswordsToExport_ = passwords.length > 0;
        };
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);

    const trustedVaultStateChanged = (state: TrustedVaultBannerState) => {
      this.trustedVaultBannerState_ = state;
    };
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getTrustedVaultBannerState().then(
        trustedVaultStateChanged);
    this.addWebUiListener(
        'trusted-vault-banner-state-changed', trustedVaultStateChanged);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setBlockedSitesListListener_);
    PasswordManagerImpl.getInstance().removeBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);
    this.setBlockedSitesListListener_ = null;

    assert(this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);
    this.setCredentialsChangedListener_ = null;
  }

  private getBlockedSitesDescription_() {
    return this.i18n(
        this.blockedSites_.length ? 'blockedSitesDescription' :
                                    'blockedSitesEmptyDescription');
  }

  private onAddShortcutClick_() {
    // TODO(crbug.com/1358448): Record metrics on all entry points usage.
    // TODO(crbug.com/1358448): Hide the button for users after the shortcut is
    // installed.
    PasswordManagerImpl.getInstance().showAddShortcutDialog();
  }

  /**
   * Fires an event that should delete the blocked password entry.
   */
  private onRemoveBlockedSiteClick_(
      event: DomRepeatEvent<chrome.passwordsPrivate.ExceptionEntry>) {
    PasswordManagerImpl.getInstance().removeBlockedSite(event.model.item.id);
  }

  // <if expr="is_win or is_macosx">
  private switchBiometricAuthBeforeFillingState_(e: Event) {
    const biometricAuthenticationForFillingToggle =
        e!.target as PrefToggleButtonElement;
    assert(biometricAuthenticationForFillingToggle);
    PasswordManagerImpl.getInstance().switchBiometricAuthBeforeFillingState();
  }
  // </if>

  private getShortcutBannerDescription_(): string {
    return this.i18n(
        'addShortcutDescription', this.i18n('localPasswordManager'));
  }

  private onTrustedVaultBannerClick_() {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultLearnMoreUrl'));
        break;
      case TrustedVaultBannerState.OFFER_OPT_IN:
        OpenWindowProxyImpl.getInstance().openUrl(
            loadTimeData.getString('trustedVaultOptInUrl'));
        break;
      case TrustedVaultBannerState.NOT_SHOWN:
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerTitle_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private getTrustedVaultBannerDescription_(): string {
    switch (this.trustedVaultBannerState_) {
      case TrustedVaultBannerState.OPTED_IN:
        return this.i18n('trustedVaultBannerSubLabelOptedIn');
      case TrustedVaultBannerState.OFFER_OPT_IN:
        return this.i18n('trustedVaultBannerSubLabelOfferOptIn');
      case TrustedVaultBannerState.NOT_SHOWN:
        return '';
      default:
        assertNotReached();
    }
  }

  private shouldHideTrustedVaultBanner_(): boolean {
    return this.trustedVaultBannerState_ === TrustedVaultBannerState.NOT_SHOWN;
  }

  private getAriaLabelForBlockedSite_(
      blockedSite: chrome.passwordsPrivate.ExceptionEntry): string {
    return this.i18n('removeBlockedAriaDescription', blockedSite.urls.shown);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);

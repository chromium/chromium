// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {PrefToggleButtonElement} from './prefs/pref_toggle_button.js';
import {getTemplate} from './settings_section.html.js';

export interface SettingsSectionElement {
  $: {
    autosigninToggle: PrefToggleButtonElement,
    blockedSitesList: HTMLElement,
    passwordToggle: PrefToggleButtonElement,
  };
}

export class SettingsSectionElement extends I18nMixin
(PolymerElement) {
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
    };
  }

  private blockedSites_: BlockedSite[];

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;
  private setCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  private addShortcutDescription_: string;

  private hasPasswordsToExport_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };
    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);

    this.addShortcutDescription_ =
        this.i18n('addShortcutDescription', this.i18n('localPasswordManager'));

    this.setCredentialsChangedListener_ =
        (passwords: chrome.passwordsPrivate.PasswordUiEntry[]) => {
          this.hasPasswordsToExport_ = passwords.length > 0;
        };
    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setCredentialsChangedListener_);
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
    return this.addShortcutDescription_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);

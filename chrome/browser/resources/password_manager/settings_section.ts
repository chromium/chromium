// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './prefs/pref_toggle_button.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BlockedSite, BlockedSitesListChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
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
    };
  }

  private blockedSites_: BlockedSite[];

  private setBlockedSitesListListener_: BlockedSitesListChangedListener|null =
      null;

  override connectedCallback() {
    super.connectedCallback();
    this.setBlockedSitesListListener_ = blockedSites => {
      this.blockedSites_ = blockedSites;
    };

    PasswordManagerImpl.getInstance().getBlockedSitesList().then(
        blockedSites => this.blockedSites_ = blockedSites);
    PasswordManagerImpl.getInstance().addBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setBlockedSitesListListener_);
    PasswordManagerImpl.getInstance().removeBlockedSitesListChangedListener(
        this.setBlockedSitesListListener_);
    this.setBlockedSitesListListener_ = null;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {FledgeState, PrivacySandboxBrowserProxy, PrivacySandboxBrowserProxyImpl, PrivacySandboxInterest} from './privacy_sandbox_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_fledge_subpage.html.js';

export interface SettingsPrivacySandboxFledgeSubpageElement {
  $: {
    fledgeToggle: SettingsToggleButtonElement,
  };
}

const SettingsPrivacySandboxFledgeSubpageElementBase =
    PrefsMixin(PolymerElement);

export class SettingsPrivacySandboxFledgeSubpageElement extends
    SettingsPrivacySandboxFledgeSubpageElementBase {
  static get is() {
    return 'settings-privacy-sandbox-fledge-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      sitesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used to determine that the Sites list was already fetched and to
       * display the current sites description only after the list is loaded,
       * to avoid displaying first the description for an empty list since the
       * array is empty at first when the page is loaded and switching to the
       * default description once the list is fetched.
       */
      isSitesListLoaded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private sitesList_: PrivacySandboxInterest[];
  private isSitesListLoaded_: boolean;
  private privacySandboxBrowserProxy_: PrivacySandboxBrowserProxy =
      PrivacySandboxBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.privacySandboxBrowserProxy_.getFledgeState().then(
        state => this.onFledgeStateChanged_(state));
  }

  private onFledgeStateChanged_(state: FledgeState) {
    this.sitesList_ = state.joiningSites.map(site => {
      return {site, removed: false};
    });
    this.isSitesListLoaded_ = true;
  }

  private isFledgeEnabledAndLoaded_(): boolean {
    return this.getPref('privacy_sandbox.m1.fledge_enabled').value &&
        this.isSitesListLoaded_;
  }

  private isSitesListEmpty_(): boolean {
    return this.sitesList_.length === 0;
  }

  private onLearnMoreClick_() {
    // TODO(b/254411472): Open Learn More dialog.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-fledge-subpage':
        SettingsPrivacySandboxFledgeSubpageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxFledgeSubpageElement.is,
    SettingsPrivacySandboxFledgeSubpageElement);

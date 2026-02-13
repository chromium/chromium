// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'manage-isolated-web-apps-page' is responsible for Isolated Web Apps related
 * controls.
 */

import '../../controls/settings_toggle_button.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PrefsState} from '../../common/types.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';

import {getTemplate} from './manage_isolated_web_apps_subpage.html.js';

const {Enforcement, ControlledBy} = chrome.settingsPrivate;

const ManageIsolatedWebAppsSubpageBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class ManageIsolatedWebAppsSubpageElement extends
    ManageIsolatedWebAppsSubpageBase {
  static get is() {
    return 'settings-manage-isolated-web-apps-subpage';
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

      /**
       * Computed property to determine IWA toggle value.
       */
      syntheticIwaPref_: {
        type: Object,
        computed:
            'computeSyntheticIwaPref_(prefs.ash.isolated_web_apps_enabled, ' +
            'prefs.profile.isolated_web_app.install.user_install_enabled)',
      },
    };
  }

  prefs: PrefsState;
  private syntheticIwaPref_: chrome.settingsPrivate.PrefObject|undefined;

  /**
   * Generates a pref object for the toggle button. If the profile policy is
   * false, it ignores user pref and shows the "off" state and policy icon.
   */
  private computeSyntheticIwaPref_(
      userPref: chrome.settingsPrivate.PrefObject|undefined,
      profilePref: chrome.settingsPrivate.PrefObject|
      undefined): chrome.settingsPrivate.PrefObject|undefined {
    if (!userPref || !profilePref) {
      return undefined;
    }

    const syntheticPref = Object.assign({}, userPref);

    if (!profilePref.value) {
      syntheticPref.value = false;
      syntheticPref.enforcement = Enforcement.ENFORCED;
      syntheticPref.controlledBy =
          profilePref.controlledBy || ControlledBy.USER_POLICY;
    }

    return syntheticPref;
  }

  private onIwaToggleChange_(event: Event): void {
    const target = event.target as SettingsToggleButtonElement;

    if (!this.prefs['profile']
             .isolated_web_app.install.user_install_enabled.value) {
      target.checked = false;
      return;
    }

    this.set('prefs.ash.isolated_web_apps_enabled.value', target.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-manage-isolated-web-apps-subpage':
        ManageIsolatedWebAppsSubpageElement;
  }
}

customElements.define(
    ManageIsolatedWebAppsSubpageElement.is,
    ManageIsolatedWebAppsSubpageElement);

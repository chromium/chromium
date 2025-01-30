// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './glic_data_page.html.js';

export enum SettingsGlicDataPageFeaturePrefName {
  SETTINGS_POLICY = 'glic.settings_policy',
  GEOLOCATION_ENABLED = 'glic.geolocation_enabled',
  MICROPHONE_ENABLED = 'glic.microphone_enabled',
  TAB_CONTEXT_ENABLED = 'glic.tab_context_enabled',
}

const SettingsGlicDataPageElementBase = PrefsMixin(PolymerElement);

export class SettingsGlicDataPageElement extends
    SettingsGlicDataPageElementBase {
  static get is() {
    return 'settings-glic-data-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      tabAccessToggleExpanded_: {
        type: Boolean,
        value: false,
      },

      // When the policy is disabled, the controls need to all show "off" so we
      // render a page with all the toggles bound to this fake pref rather than
      // real pref which could be either value.
      fakePref_: {
        type: Object,
        value: {
          key: 'glic.fake_pref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: 0,
        },
      },
    };
  }

  private fakePref_: chrome.settingsPrivate.PrefObject;
  private tabAccessToggleExpanded_: boolean;

  private isEnabledByPolicy_(): boolean {
    return this.getPref<number>(
                   SettingsGlicDataPageFeaturePrefName.SETTINGS_POLICY)
               .value === 0;
  }

  private onTabAccessToggleChange_(event: CustomEvent<{value: boolean}>) {
    this.tabAccessToggleExpanded_ = event.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-data-page': SettingsGlicDataPageElement;
  }
}

customElements.define(
    SettingsGlicDataPageElement.is, SettingsGlicDataPageElement);

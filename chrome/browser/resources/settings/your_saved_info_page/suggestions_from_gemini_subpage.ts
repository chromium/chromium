// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_subpage.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
import '../autofill_page/your_saved_info_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './suggestions_from_gemini_subpage.html.js';

const SettingsSuggestionsFromGeminiSubpageElementBase =
    SettingsViewMixin(PrefsMixin(PolymerElement));

interface AtMemoryTriggerPrefValue {
  is_shortcut: boolean;
  trigger: string;
}

const atMemoryTriggerPrefName = 'autofill.at_memory.trigger_info';

export class SettingsSuggestionsFromGeminiSubpageElement extends
    SettingsSuggestionsFromGeminiSubpageElementBase {
  static get is() {
    return 'settings-suggestions-from-gemini-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      isAtMemoryEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAtMemoryEnabled');
        },
      },

      atMemoryTrigger_: {
        type: String,
        computed:
            `computeAtMemoryTrigger_(prefs.${atMemoryTriggerPrefName}.value)`,
      },
    };
  }

  declare prefs: Record<string, unknown>;
  declare private isAtMemoryEnabled_: boolean;
  declare private atMemoryTrigger_: string;

  private showQualityLogging_(toggleOn: boolean, atMemoryEnabled: boolean):
      boolean {
    return toggleOn && atMemoryEnabled;
  }
  private onManageConnectedAppsClick_() {
    // TODO(crbug.com/512204278): Add metrics.
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('personalContextConnectedAppsUrl'));
  }

  private onAtMemoryTriggerSettingUpdated_(event: CustomEvent<string>) {
    const newTrigger = event.detail;
    if (newTrigger === '') {
      this.setPrefValue(
          atMemoryTriggerPrefName, {is_shortcut: false, trigger: '@@'});
    } else {
      this.setPrefValue(
          atMemoryTriggerPrefName, {is_shortcut: true, trigger: newTrigger});
    }
  }

  private computeAtMemoryTrigger_(triggerPrefValue: AtMemoryTriggerPrefValue):
      string {
    if (!triggerPrefValue.is_shortcut) {
      return '';
    }
    return triggerPrefValue.trigger;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-suggestions-from-gemini-subpage':
        SettingsSuggestionsFromGeminiSubpageElement;
  }
}

customElements.define(
    SettingsSuggestionsFromGeminiSubpageElement.is,
    SettingsSuggestionsFromGeminiSubpageElement);

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import '../../settings_columned_section.css.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import '../autofill_shared.css.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModelExecutionEnterprisePolicyValue} from '../../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiAction} from '../../metrics_browser_proxy.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';

import {getTemplate} from './suggestions_from_gemini_page.html.js';

const SettingsSuggestionsFromGeminiPageElementBase =
    SettingsViewMixin(PrefServiceObserverMixin(PolymerElement));

const AT_MEMORY_SHORTCUT_PREF_NAME = 'autofill.at_memory.shortcut';

export interface SettingsSuggestionsFromGeminiPageElement {
  $: {
    atMemoryDoubleCtrlTriggerToggle: SettingsToggleButtonElement,
  };
}

export class SettingsSuggestionsFromGeminiPageElement extends
    SettingsSuggestionsFromGeminiPageElementBase {
  static get is() {
    return 'settings-suggestions-from-gemini-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isAtMemoryEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAtMemoryEnabled');
        },
      },

      isAtMemoryTriggerCustomizationAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAtMemoryTriggerCustomizationAllowed');
        },
      },

      findAndFillWithGeminiPref_: Object,

      atMemoryShortcutPref_: Object,

      findAndFillWithGeminiSettingsPref_: Object,
    };
  }

  declare private isAtMemoryEnabled_: boolean;
  declare private isAtMemoryTriggerCustomizationAllowed_: boolean;
  declare private findAndFillWithGeminiPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  declare private atMemoryShortcutPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  declare private findAndFillWithGeminiSettingsPref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'generated.find_and_fill_with_gemini': 'findAndFillWithGeminiPref_',
      [AT_MEMORY_SHORTCUT_PREF_NAME]: 'atMemoryShortcutPref_',
      'autofill.personal_context.find_and_fill_with_gemini_settings':
          'findAndFillWithGeminiSettingsPref_',
    });
  }

  private showQualityLogging_(): boolean {
    return !!this.findAndFillWithGeminiPref_?.value && this.isAtMemoryEnabled_;
  }

  private showDoubleCtrlShortcut_(): boolean {
    return this.isAtMemoryTriggerCustomizationAllowed_ &&
        !!this.findAndFillWithGeminiPref_?.value;
  }

  private showConsiderNoLoggingEnterprise_(): boolean {
    return this.findAndFillWithGeminiSettingsPref_?.value ===
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING;
  }
  private onManageConnectedAppsClick_() {
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiAction(
        SuggestionsFromGeminiAction.MANAGE_CONNECTED_APPS_CLICK);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('personalContextConnectedAppsUrl'));
  }

  private onToggleChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiAction(
        toggle.checked ? SuggestionsFromGeminiAction.TOGGLE_ON :
                         SuggestionsFromGeminiAction.TOGGLE_OFF);
  }

  private onAtMemoryShortcutUpdated_(event: CustomEvent<string>) {
    PrefService.getInstance().setPrefValue(
        AT_MEMORY_SHORTCUT_PREF_NAME, event.detail);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-suggestions-from-gemini-page':
        SettingsSuggestionsFromGeminiPageElement;
  }
}

customElements.define(
    SettingsSuggestionsFromGeminiPageElement.is,
    SettingsSuggestionsFromGeminiPageElement);

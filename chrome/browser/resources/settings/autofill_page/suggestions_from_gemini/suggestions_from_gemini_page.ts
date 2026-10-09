// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import '../../settings_page/settings_subpage.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {getCss as getCrIconsCss} from 'chrome://resources/cr_elements/cr_icons_lit.css.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ModelExecutionEnterprisePolicyValue} from '../../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiAction} from '../../metrics_browser_proxy.js';
import {getCss as getSettingsColumnedSectionCss} from '../../settings_columned_section_lit.css.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../../settings_shared_lit.css.js';
import {getCss as getAutofillSharedCss} from '../autofill_shared_lit.css.js';

import {getHtml} from './suggestions_from_gemini_page.html.js';

const SettingsSuggestionsFromGeminiPageElementBase =
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement));

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

  static override get styles() {
    return [
      getAutofillSharedCss(),
      getCrIconsCss(),
      getCrSharedStyleCss(),
      getSettingsColumnedSectionCss(),
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isAtMemoryEnabled_: {type: Boolean},
      isAtMemoryTriggerCustomizationAllowed_: {type: Boolean},
      findAndFillWithGeminiPref_: {type: Object},
      atMemoryShortcutPref_: {type: Object},
      findAndFillWithGeminiSettingsPref_: {type: Object},
    };
  }

  private accessor isAtMemoryEnabled_: boolean =
      loadTimeData.getBoolean('isAtMemoryEnabled');
  private accessor isAtMemoryTriggerCustomizationAllowed_: boolean =
      loadTimeData.getBoolean('isAtMemoryTriggerCustomizationAllowed');
  protected accessor findAndFillWithGeminiPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor atMemoryShortcutPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  protected accessor findAndFillWithGeminiSettingsPref_:
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

  protected showQualityLogging_(): boolean {
    return !!this.findAndFillWithGeminiPref_?.value && this.isAtMemoryEnabled_;
  }

  protected showDoubleCtrlShortcut_(): boolean {
    return this.isAtMemoryTriggerCustomizationAllowed_ &&
        !!this.findAndFillWithGeminiPref_?.value;
  }

  protected showConsiderNoLoggingEnterprise_(): boolean {
    return this.findAndFillWithGeminiSettingsPref_?.value ===
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING;
  }

  protected onManageConnectedAppsClick_() {
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiAction(
        SuggestionsFromGeminiAction.MANAGE_CONNECTED_APPS_CLICK);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('personalContextConnectedAppsUrl'));
  }

  protected onToggleSettingsBooleanControlChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    this.metricsBrowserProxy_.recordSuggestionsFromGeminiAction(
        toggle.checked ? SuggestionsFromGeminiAction.TOGGLE_ON :
                         SuggestionsFromGeminiAction.TOGGLE_OFF);
  }

  protected onAtMemoryShortcutUpdated_(event: CustomEvent<string>) {
    PrefService.getInstance().setPrefValue(
        AT_MEMORY_SHORTCUT_PREF_NAME, event.detail);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
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

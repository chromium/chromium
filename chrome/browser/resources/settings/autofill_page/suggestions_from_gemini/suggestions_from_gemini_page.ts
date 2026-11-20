// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_subpage.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiAction} from '../../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../../settings_shared_lit.css.js';
import {getCss as getAutofillSharedCss} from '../autofill_shared_lit.css.js';

import {getHtml} from './suggestions_from_gemini_page.html.js';

const SettingsSuggestionsFromGeminiPageElementBase =
    SettingsViewMixinLit(CrLitElement);

export class SettingsSuggestionsFromGeminiPageElement extends
    SettingsSuggestionsFromGeminiPageElementBase {
  static get is() {
    return 'settings-suggestions-from-gemini-page';
  }

  static override get styles() {
    return [
      getAutofillSharedCss(),
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

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

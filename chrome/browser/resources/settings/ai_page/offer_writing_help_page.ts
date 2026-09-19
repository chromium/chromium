// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './ai_logging_info_bullet.js';
import './ai_policy_indicator.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageComposeInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getAiLearnMoreUrl} from './ai_learn_more_url_util.js';
import {AiEnterpriseFeaturePrefName, AiPageActions} from './constants.js';
import {getCss} from './offer_writing_help_page.css.js';
import {getHtml} from './offer_writing_help_page.html.js';

export const COMPOSE_PROACTIVE_NUDGE_PREF = 'compose.proactive_nudge_enabled';
export const COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF =
    'compose.proactive_nudge_disabled_sites_with_time';

const SettingsOfferWritingHelpPageElementBase = SettingsViewMixinLit(
    I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement)));

export class SettingsOfferWritingHelpPageElement extends
    SettingsOfferWritingHelpPageElementBase {
  static get is() {
    return 'settings-offer-writing-help-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      siteList_: {type: Array},
      enableComposeProactiveNudge_: {type: Boolean},
      enterprisePref_: {type: Object},
    };
  }

  protected accessor siteList_: string[] = [];
  protected accessor enableComposeProactiveNudge_: boolean =
      loadTimeData.getBoolean('enableComposeProactiveNudge');
  protected accessor enterprisePref_: chrome.settingsPrivate.PrefObject|
      undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(AiEnterpriseFeaturePrefName.COMPOSE, 'enterprisePref_');
    this.addPrefObserver(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF,
        () => this.onPrefsChanged_());
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private recordInteractionMetrics_(
      interaction: AiPageComposeInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageComposeInteractions(interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  protected onLearnMoreClick_() {
    this.recordInteractionMetrics_(
        AiPageComposeInteractions.LEARN_MORE_LINK_CLICKED,
        AiPageActions.COMPOSE_LEARN_MORE_CLICKED);
  }

  protected onComposeProactiveNudgeToggleSettingsBooleanControlChange_(
      e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    if (toggle.checked) {
      this.recordInteractionMetrics_(
          AiPageComposeInteractions.COMPOSE_PROACTIVE_NUDGE_ENABLED,
          AiPageActions.COMPOSE_PROACTIVE_NUDGE_ENABLED);
      return;
    }
    this.recordInteractionMetrics_(
        AiPageComposeInteractions.COMPOSE_PROACTIVE_NUDGE_DISABLED,
        AiPageActions.COMPOSE_PROACTIVE_NUDGE_DISABLED);
  }

  protected hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  protected onDeleteClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    PrefService.getInstance().deletePrefDictEntry(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, target.dataset['site']!);
  }

  private onPrefsChanged_() {
    const prefDict = PrefService.getInstance()
                         .getPref<Record<string, number>>(
                             COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF)
                         .value;
    this.siteList_ = Object.keys(prefDict);
  }

  protected getLearnMoreUrl_(): string {
    return getAiLearnMoreUrl(
        this.enterprisePref_, loadTimeData.getString('composeLearnMorePageURL'),
        loadTimeData.getString('composeLearnMorePageManagedURL'));
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

export type OfferWritingHelpPageElement = SettingsOfferWritingHelpPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-offer-writing-help-page': SettingsOfferWritingHelpPageElement;
  }
}

customElements.define(
    SettingsOfferWritingHelpPageElement.is,
    SettingsOfferWritingHelpPageElement);

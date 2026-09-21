// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_page/settings_section.js';
import '../privacy_icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './ai_page.css.js';
import {getHtml} from './ai_page.html.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName} from './constants.js';
// <if expr="_google_chrome">
import type {OnDeviceAiBrowserProxy, OnDeviceAiEnabled} from './on_device_ai_browser_proxy.js';
import {OnDeviceAiBrowserProxyImpl} from './on_device_ai_browser_proxy.js';
// </if>

const SettingsAiPageElementBase = WebUiListenerMixinLit(
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement)));

export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showComposeControl_: {type: Boolean},
      showHistorySearchControl_: {type: Boolean},
      showPasswordChangeControl_: {type: Boolean},
      showAiSuggestionsControl_: {type: Boolean},
      showInlineCueMenuControl_: {type: Boolean},
      showSkillsSettingPage_: {type: Boolean},
      showIndigoControl_: {type: Boolean},
      showGoogleSearchAiModeWorkspaceControl_: {type: Boolean},
      showDictationControl_: {type: Boolean},
      historySearchPref_: {type: Object},
      // <if expr="_google_chrome">
      showOnDeviceAiSettings_: {type: Boolean},
      onDeviceAiPref_: {type: Object},
      // </if>
    };
  }

  private accessor historySearchPref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;
  protected accessor showComposeControl_: boolean =
      loadTimeData.getBoolean('showComposeControl');
  protected accessor showHistorySearchControl_: boolean =
      loadTimeData.getBoolean('showHistorySearchControl');
  protected accessor showPasswordChangeControl_: boolean =
      loadTimeData.getBoolean('showPasswordChangeControl');
  protected accessor showAiSuggestionsControl_: boolean =
      loadTimeData.getBoolean('showAiSuggestionsControl');
  protected accessor showInlineCueMenuControl_: boolean =
      loadTimeData.getBoolean('showInlineCueMenuControl');
  protected accessor showSkillsSettingPage_: boolean =
      loadTimeData.getBoolean('showSkillsSettingPage');
  protected accessor showIndigoControl_: boolean =
      loadTimeData.getBoolean('showIndigoControl');
  protected accessor showGoogleSearchAiModeWorkspaceControl_: boolean =
      loadTimeData.getBoolean('showGoogleSearchAiModeWorkspaceControl');
  protected accessor showDictationControl_: boolean =
      loadTimeData.getBoolean('showDictationControl');
  // <if expr="_google_chrome">
  protected accessor showOnDeviceAiSettings_: boolean =
      loadTimeData.getBoolean('showOnDeviceAiSettings');
  protected accessor onDeviceAiPref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'settings.on_device_ai_enabled',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };
  // </if>

  private shouldRecordMetrics_: boolean = true;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  // <if expr="_google_chrome">
  private onDeviceAiBrowserProxy_: OnDeviceAiBrowserProxy =
      OnDeviceAiBrowserProxyImpl.getInstance();
  // </if>

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(
        SettingsAiPageFeaturePrefName.HISTORY_SEARCH, 'historySearchPref_');
    this.maybeLogVisibilityMetrics_();
    // <if expr="_google_chrome">
    const setOnDeviceAiPref = (onDeviceAiEnabled: OnDeviceAiEnabled) =>
        this.setOnDeviceAiPref_(onDeviceAiEnabled);
    this.addWebUiListener('on-device-ai-enabled-changed', setOnDeviceAiPref);
    this.onDeviceAiBrowserProxy_.getOnDeviceAiEnabled().then(setOnDeviceAiPref);
    // </if>
  }

  private maybeLogVisibilityMetrics_() {
    // Only record metrics when the user first navigates to the main AI page.
    if (!this.shouldRecordMetrics_ ||
        Router.getInstance().getCurrentRoute() !== routes.AI) {
      return;
    }
    this.shouldRecordMetrics_ = false;

    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.HistorySearch',
        this.showHistorySearchControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.Compose', this.showComposeControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.PasswordChange',
        this.showPasswordChangeControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.AiSuggestions',
        this.showAiSuggestionsControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.Indigo', this.showIndigoControl_);
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.GoogleSearchAiModeWorkspace',
        this.showGoogleSearchAiModeWorkspaceControl_);
  }

  protected onHistorySearchRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.HISTORY_SEARCH_CLICK,
        'Settings.AiPage.HistorySearchEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().HISTORY_SEARCH);
  }

  protected onComposeRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.COMPOSE_CLICK,
        'Settings.AiPage.ComposeEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().OFFER_WRITING_HELP);
  }

  protected onPasswordChangeRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.PASSWORD_CHANGE_CLICK,
        'Settings.AiPage.PasswordChangeEntryPointClick');

    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('passwordChangeSettingsUrl'));
  }

  protected onAiSuggestionsRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.AI_SUGGESTIONS_CLICK,
        'Settings.AiPage.AiSuggestionsEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().AI_SUGGESTIONS);
  }

  protected onInlineCueMenuRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.INLINE_CUE_MENU_CLICK,
        'Settings.AiPage.InlineCueMenuEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().INLINE_CUE_MENU);
  }

  protected onSkillsRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.SKILLS_CLICK,
        'Settings.AiPage.SkillsEntryPointClick');

    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SKILLS);
  }

  protected onDictationRowClick_() {
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().DICTATION);
  }

  protected onIndigoRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.INDIGO_CLICK,
        'Settings.AiPage.IndigoEntryPointClick');

    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('indigoSavedUrl'));
  }

  protected onGoogleSearchAiModeWorkspaceRowClick_() {
    this.recordInteractionMetrics_(
        AiPageInteractions.GOOGLE_SEARCH_AI_MODE_WORKSPACE_CLICK,
        'Settings.AiPage.GoogleSearchAiModeWorkspaceEntryPointClick');

    let isRestricted = false;
    try {
      const consentState =
          PrefService.getInstance()
              .getPref<number>('contextual_search.drive_consent_state')
              .value;
      isRestricted = consentState === 1;  // DriveConsentState::kRestricted
    } catch (e) {
      console.error(
          'Failed to read contextual_search.drive_consent_state pref:', e);
    }

    let url;
    try {
      url = loadTimeData.getString(
          isRestricted ? 'googleSearchAiModeRestrictedUrl' :
                         'googleSearchAiModeWorkspaceUrl');
    } catch (e) {
      console.error('Failed to read URL from loadTimeData:', e);
      return;
    }

    OpenWindowProxyImpl.getInstance().openUrl(url);
  }


  private recordInteractionMetrics_(
      interaction: AiPageInteractions, action: string) {
    this.metricsBrowserProxy_.recordAiPageInteractions(interaction);
    this.metricsBrowserProxy_.recordAction(action);
  }

  protected getHistorySearchSublabel_(): string {
    const isAnswersEnabled =
        loadTimeData.getBoolean('historyEmbeddingsAnswersFeatureEnabled');
    if (this.historySearchPref_?.value === FeatureOptInState.ENABLED) {
      return isAnswersEnabled ?
          loadTimeData.getString('historySearchWithAnswersSublabelOn') :
          loadTimeData.getString('historySearchSublabelOn');
    }
    return isAnswersEnabled ?
        loadTimeData.getString('historySearchWithAnswersSublabelOff') :
        loadTimeData.getString('historySearchSublabelOff');
  }

  // <if expr="_google_chrome">
  protected onOnDeviceAiSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('onDeviceAiLearnMoreUrl'));
  }

  protected onOnDeviceAiSendFeedback_(e: Event) {
    e.stopPropagation();
    this.onDeviceAiBrowserProxy_.openFeedbackDialog();
  }

  protected onOnDeviceAiSettingsBooleanControlChange_(e: Event) {
    const enabled = (e.target as SettingsToggleButtonElement).checked;
    this.onDeviceAiBrowserProxy_.setOnDeviceAiEnabled(enabled);
  }

  private setOnDeviceAiPref_(onDeviceAiEnabled: OnDeviceAiEnabled) {
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'settings.on_device_ai_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: onDeviceAiEnabled.enabled,
    };

    if (!onDeviceAiEnabled.allowedByPolicy) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      pref.value = false;
    }

    this.onDeviceAiPref_ = pref;
  }
  // </if>

  // SettingsViewMixinLit implementation.
  override getFocusConfig() {
    const map = new Map();

    if (routes.HISTORY_SEARCH) {
      map.set(routes.HISTORY_SEARCH.path, '#historySearchRowV2');
    }

    if (routes.OFFER_WRITING_HELP) {
      map.set(routes.OFFER_WRITING_HELP.path, '#composeRowV2');
    }

    if (routes.AI_SUGGESTIONS) {
      map.set(routes.AI_SUGGESTIONS.path, '#aiSuggestionsRow');
    }

    if (routes.INLINE_CUE_MENU) {
      map.set(routes.INLINE_CUE_MENU.path, '#inlineCueMenuRow');
    }

    if (routes.SKILLS) {
      map.set(routes.SKILLS.path, '#skillsRow');
    }

    if (routes.DICTATION) {
      map.set(routes.DICTATION.path, '#dictationRow');
    }

    return map;
  }

  // SettingsViewMixinLit implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    const ids = [
      'compose',
      'dictation',
      'historySearch',
      'aiSuggestions',
      'inlineCueMenu',
      'skills',
    ];
    assert(ids.includes(childViewId));

    let triggerId: string|null = null;
    switch (childViewId) {
      case 'compose':
        assert(this.showComposeControl_);
        triggerId = 'composeRowV2';
        break;
      case 'historySearch':
        assert(this.showHistorySearchControl_);
        triggerId = 'historySearchRowV2';
        break;
      case 'aiSuggestions':
        assert(this.showAiSuggestionsControl_);
        triggerId = 'aiSuggestionsRow';
        break;
      case 'inlineCueMenu':
        assert(this.showInlineCueMenuControl_);
        triggerId = 'inlineCueMenuRow';
        break;
      case 'skills':
        assert(this.showSkillsSettingPage_);
        triggerId = 'skillsRow';
        break;
      case 'dictation':
        assert(this.showDictationControl_);
        triggerId = 'dictationRow';
        break;
      default:
        assertNotReached();
    }

    assert(triggerId);

    const control = this.shadowRoot.querySelector<HTMLElement>(`#${triggerId}`);
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }
}

export type AiPageElement = SettingsAiPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page': SettingsAiPageElement;
  }
}

customElements.define(SettingsAiPageElement.is, SettingsAiPageElement);

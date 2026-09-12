// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_subpage.js';
import './glic_login_permissions_page.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AiPageActions} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {GlicBrowserProxy} from './glic_browser_proxy.js';
import {GlicBrowserProxyImpl} from './glic_browser_proxy.js';
import {getCss} from './glic_subpage.css.js';
import {getHtml} from './glic_subpage.html.js';

export enum SettingsGlicPageFeaturePrefName {
  CLOSED_CAPTIONS_ENABLED = 'glic.closed_captioning_enabled',
  MEDIA_UNDERSTANDING_ENABLED = 'glic.media_understanding_enabled',
  GEOLOCATION_ENABLED = 'glic.geolocation_enabled',
  LAUNCHER_ENABLED = 'glic.launcher_enabled',
  MICROPHONE_ENABLED = 'glic.microphone_enabled',
  SETTINGS_POLICY = 'browser.gemini_settings',
  TAB_CONTEXT_ENABLED = 'glic.tab_context_enabled',
  TABSTRIP_BUTTON_ENABLED = 'glic.pinned_to_tabstrip',
  USER_STATUS = 'glic.user_status',
  DEFAULT_TAB_CONTEXT_ENABLED = 'glic.default_tab_context_enabled',
  WEB_ACTUATION_ENABLED = 'glic.user_enabled_actuation_on_web',
  EXPERIMENTAL_TRIGGERING_ENABLED = 'glic.experimental_triggering_enabled',
  KEEP_SIDEPANEL_OPEN_ON_NEW_TABS_ENABLED =
      'glic.keep_sidepanel_open_on_new_tabs_enabled',
  SHAKE_TRIGGER_ENABLED = 'glic.shake_trigger_enabled',
  HOTKEY_GLOBAL_SCOPE_ENABLED = 'glic.hotkey_global_scope_enabled',
}

// browser_element_identifiers constants
const OS_WIDGET_TOGGLE_ELEMENT_ID = 'kGlicOsToggleElementId';
const OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID =
    'kGlicOsWidgetKeyboardShortcutElementId';

// Partial structure of the glic.user_status dictionary pref.
interface GlicUserStatusPref {
  isEnterpriseAccountDataProtected?: boolean;
}

const SettingsGlicSubpageElementBase =
    SettingsViewMixinLit(HelpBubbleMixinLit(I18nMixinLit(
        WebUiListenerMixinLit(PrefServiceObserverMixinLit(CrLitElement)))));

export type GlicSubpageElement = SettingsGlicSubpageElement;

export class SettingsGlicSubpageElement extends SettingsGlicSubpageElementBase {
  static get is() {
    return 'settings-glic-subpage';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disallowedByAdmin_: {type: Boolean},
      userStatusPref_: {type: Object},
      launcherEnabledPref_: {type: Object},
      hotkeyGlobalScopeEnabledPref_: {type: Object},
      selectedScope_: {type: String},
      registeredShortcut_: {type: String},
      registeredFocusToggleShortcut_: {type: String},
      registeredSelectionShortcut_: {type: String},
      tabAccessToggleExpanded_: {type: Boolean},
      fakePref_: {type: Object},
      closedCaptionsToggleEnabled_: {type: Boolean},
      headlessCaptionsEnabled_: {type: Boolean},
      glicExtensionsFeatureEnabled_: {type: Boolean},
      glicUserStatusCheckFeatureEnabled_: {type: Boolean},
      glicSelectionFeatureEnabled_: {type: Boolean},
      glicHotkeyLocalScopeEnabled_: {type: Boolean},
      showGlicDefaultTabContextSetting_: {type: Boolean},
      showGlicExperimentalTriggering_: {type: Boolean},
      experimentalTriggeringExpanded_: {type: Boolean},
      showGlicPersonalContextLink_: {type: Boolean},
      showGlicInstructionLink_: {type: Boolean},
      showGlicKeepSidepanelOpenOnNewTabsSetting_: {type: Boolean},
      showGlicShakeTrigger_: {type: Boolean},
      microphoneToggleEnabled_: {type: Boolean},
      defaultTabAccessToggleExpanded_: {type: Boolean},
      webActuationFeatureEnabled_: {type: Boolean},
      webActuationEnabledPref_: {type: Object},
      experimentalTriggeringEnabledPref_: {type: Object},
      isWebActuationDisabledForEnterprise_: {type: Boolean},
      webActuationDisabledForEnterprisePref_: {type: Object},
      webActuationEnabledExpanded_: {type: Boolean},
      actorLoginFederatedLoginSupportEnabled_: {type: Boolean},
    };
  }

  protected accessor disallowedByAdmin_: boolean = false;
  protected accessor userStatusPref_:
      chrome.settingsPrivate.PrefObject<GlicUserStatusPref>|
      undefined = undefined;
  protected accessor launcherEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined = undefined;
  protected accessor hotkeyGlobalScopeEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined = undefined;
  protected accessor selectedScope_: 'GLOBAL'|'CHROME' = 'CHROME';
  protected accessor registeredShortcut_: string = '';
  protected accessor registeredFocusToggleShortcut_: string = '';
  protected accessor registeredSelectionShortcut_: string = '';
  protected accessor tabAccessToggleExpanded_: boolean = false;
  protected accessor fakePref_: chrome.settingsPrivate.PrefObject = {
    key: 'glic.fake_pref',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: 0,
  };
  protected accessor closedCaptionsToggleEnabled_: boolean =
      loadTimeData.getBoolean('glicCanUseLive');
  protected accessor headlessCaptionsEnabled_: boolean =
      loadTimeData.getBoolean('headlessCaptionsEnabled');
  protected accessor glicExtensionsFeatureEnabled_: boolean =
      loadTimeData.getBoolean('glicExtensionsFeatureEnabled');
  protected accessor glicUserStatusCheckFeatureEnabled_: boolean =
      loadTimeData.getBoolean('glicUserStatusCheckFeatureEnabled');
  protected accessor glicSelectionFeatureEnabled_: boolean =
      loadTimeData.getBoolean('glicSelectionFeatureEnabled');
  protected accessor glicHotkeyLocalScopeEnabled_: boolean =
      loadTimeData.getBoolean('glicHotkeyLocalScopeEnabled');
  protected accessor showGlicDefaultTabContextSetting_: boolean =
      loadTimeData.getBoolean('showGlicDefaultTabContextSetting');
  protected accessor showGlicExperimentalTriggering_: boolean =
      loadTimeData.getBoolean('showGlicExperimentalTriggering');
  protected accessor experimentalTriggeringExpanded_: boolean = false;
  protected accessor showGlicPersonalContextLink_: boolean =
      loadTimeData.getBoolean('showGeminiPersonalContextLink');
  protected accessor showGlicInstructionLink_: boolean =
      loadTimeData.getBoolean('showInstructionLink');
  protected accessor showGlicKeepSidepanelOpenOnNewTabsSetting_: boolean =
      loadTimeData.getBoolean('showGlicKeepSidepanelOpenOnNewTabsSetting');
  protected accessor showGlicShakeTrigger_: boolean =
      loadTimeData.getBoolean('showGlicShakeTrigger');
  protected accessor microphoneToggleEnabled_: boolean =
      loadTimeData.getBoolean('glicCanUseLive');
  protected accessor defaultTabAccessToggleExpanded_: boolean = false;
  protected accessor webActuationFeatureEnabled_: boolean = false;
  protected accessor webActuationEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'glic.web_actuation_enabled',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  protected accessor experimentalTriggeringEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'glic.experimental_triggering_enabled',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };
  protected accessor isWebActuationDisabledForEnterprise_: boolean =
      loadTimeData.getBoolean('isWebActuationDisabledForEnterprise');
  protected accessor webActuationDisabledForEnterprisePref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'glic.web_actuation_disabled_for_enterprise',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
    enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
  };
  protected accessor webActuationEnabledExpanded_: boolean = false;
  protected accessor actorLoginFederatedLoginSupportEnabled_: boolean =
      loadTimeData.getBoolean('actorLoginFederatedLoginSupportEnabled');

  private shortcutInput_: string = '';
  private focusToggleShortcutInput_: string = '';
  private selectionShortcutInput_: string = '';
  private removedShortcut_: string|null = null;
  private browserProxy_: GlicBrowserProxy = GlicBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPrefs({
      [SettingsGlicPageFeaturePrefName.USER_STATUS]: 'userStatusPref_',
      [SettingsGlicPageFeaturePrefName.LAUNCHER_ENABLED]:
          'launcherEnabledPref_',
      [SettingsGlicPageFeaturePrefName.HOTKEY_GLOBAL_SCOPE_ENABLED]:
          'hotkeyGlobalScopeEnabledPref_',
    });
    this.addPrefObserver(
        SettingsGlicPageFeaturePrefName.TAB_CONTEXT_ENABLED,
        (pref: chrome.settingsPrivate.PrefObject<boolean>) => {
          this.tabAccessToggleExpanded_ = pref.value;
        });
    this.addPrefObserver(
        SettingsGlicPageFeaturePrefName.DEFAULT_TAB_CONTEXT_ENABLED,
        (pref: chrome.settingsPrivate.PrefObject<boolean>) => {
          this.defaultTabAccessToggleExpanded_ = pref.value;
        });

    this.browserProxy_.getDisallowedByAdmin().then(
        this.disallowedByAdminChanged_.bind(this));
    this.addWebUiListener(
        'glic-disallowed-by-admin-changed',
        this.disallowedByAdminChanged_.bind(this));
    this.addWebUiListener(
        'glic-web-actuation-capability-changed',
        (canActOnWeb: boolean) =>
            this.onWebActuationCapabilityChanged_(canActOnWeb));
    this.addWebUiListener(
        'glic-web-actuation-toggle-visibility-changed',
        (visible: boolean) =>
            this.onWebActuationToggleVisibilityChanged_(visible));
    this.addWebUiListener(
        'glic-web-actuation-enabled-changed', (enabled: boolean) => {
          this.webActuationEnabledPref_ = {
            ...this.webActuationEnabledPref_,
            value: enabled,
          };
        });
    this.addWebUiListener(
        'glic-experimental-triggering-enabled-changed', (enabled: boolean) => {
          this.experimentalTriggeringEnabledPref_ = {
            ...this.experimentalTriggeringEnabledPref_,
            value: enabled,
          };
        });

    this.browserProxy_.getWebActuationToggleVisibility().then(
        (visible: boolean) => {
            this.onWebActuationToggleVisibilityChanged_(visible);
        });

    this.browserProxy_.getWebActuationEnabled().then((enabled: boolean) => {
      this.webActuationEnabledPref_ = {
        ...this.webActuationEnabledPref_,
        value: enabled,
      };
    });

    this.browserProxy_.getExperimentalTriggeringEnabled().then(
        (enabled: boolean) => {
          this.experimentalTriggeringEnabledPref_ = {
            ...this.experimentalTriggeringEnabledPref_,
            value: enabled,
          };
        });

    this.browserProxy_.getGlicShortcut().then(shortcut => {
      this.registeredShortcut_ = shortcut;
    });
    this.browserProxy_.getGlicFocusToggleShortcut().then(shortcut => {
      this.registeredFocusToggleShortcut_ = shortcut;
    });
    this.browserProxy_.getGlicSelectionShortcut().then(shortcut => {
      this.registeredSelectionShortcut_ = shortcut;
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('hotkeyGlobalScopeEnabledPref_')) {
      this.selectedScope_ = this.computeSelectedScope_(
          !!this.hotkeyGlobalScopeEnabledPref_?.value);
    }

    if (changedPrivateProperties.has('webActuationEnabledPref_')) {
      this.onWebActuationEnabledChanged_(this.webActuationEnabledPref_.value);
    }

    if (changedPrivateProperties.has('experimentalTriggeringEnabledPref_')) {
      this.onExperimentalTriggeringEnabledChanged_(
          this.experimentalTriggeringEnabledPref_.value);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('disallowedByAdmin_')) {
      this.onDisallowedByAdminChanged_();
    }
  }

  private async onDisallowedByAdminChanged_() {
    await PrefService.getInstance().whenInitialized();
    if (this.disallowedByAdmin_) {
      return;
    }

    const launcherToggle =
        this.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#launcherToggle');
    const shortcutInput = this.shadowRoot.querySelector<CrShortcutInputElement>(
        '#mainShortcutSetting .shortcut-input');
    assert(launcherToggle);
    assert(shortcutInput);

    this.registerHelpBubble(
        OS_WIDGET_TOGGLE_ELEMENT_ID, launcherToggle.getBubbleAnchor());
    this.registerHelpBubble(
        OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID, shortcutInput);
  }

  protected onLauncherToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.browserProxy_.setGlicOsLauncherEnabled(enabled);
    this.metricsBrowserProxy_.recordAction(
        'GlicOsEntrypoint.Settings.Toggle' +
        (enabled ? '.Enabled' : '.Disabled'));
    this.hideHelpBubble(OS_WIDGET_TOGGLE_ELEMENT_ID);
  }

  private computeSelectedScope_(globalEnabled: boolean): 'GLOBAL'|'CHROME' {
    return globalEnabled ? 'GLOBAL' : 'CHROME';
  }

  protected getMainShortcutOpened_(): boolean {
    return this.launcherEnabledPref_?.value ||
        this.glicHotkeyLocalScopeEnabled_;
  }

  protected onScopeChange_(event: Event) {
    const select = event.target as HTMLSelectElement;
    const isGlobal = select.value === 'GLOBAL';
    PrefService.getInstance().setPrefValue(
        SettingsGlicPageFeaturePrefName.HOTKEY_GLOBAL_SCOPE_ENABLED, isGlobal);
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.HotkeyScope.' + (isGlobal ? 'Global' : 'Chrome'));
  }

  protected onGeolocationToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.Geolocation' + (enabled ? '.Enabled' : '.Disabled'));
  }

  protected onMicrophoneToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.Microphone' + (enabled ? '.Enabled' : '.Disabled'));
  }

  protected async onShortcutUpdated_(event: CustomEvent<string>) {
    this.shortcutInput_ = event.detail;
    if (this.removedShortcut_ === null) {
      this.removedShortcut_ = this.registeredShortcut_;
    }
    await this.browserProxy_.setGlicShortcut(this.shortcutInput_);
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
    // Records true if the shortcut string is defined and not empty.
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Glic.OsEntrypoint.Settings.Shortcut', !!this.shortcutInput_);
    this.hideHelpBubble(OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID);
  }

  protected async onFocusToggleShortcutUpdated_(event: CustomEvent<string>) {
    this.focusToggleShortcutInput_ = event.detail;
    await this.browserProxy_.setGlicFocusToggleShortcut(
        this.focusToggleShortcutInput_);
    // Update the shortcut to reflect what the browser proxy returns. This
    // ensures that the displayed shortcut is accurate in the event that
    // registration failed.
    this.registeredFocusToggleShortcut_ =
        await this.browserProxy_.getGlicFocusToggleShortcut();
    // Records true if the shortcut string is defined and not empty.
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Glic.Focus.Settings.Shortcut.Customized',
        !!this.focusToggleShortcutInput_);
  }

  protected async onSelectionShortcutUpdated_(event: CustomEvent<string>) {
    this.selectionShortcutInput_ = event.detail;
    await this.browserProxy_.setGlicSelectionShortcut(
        this.selectionShortcutInput_);
    this.registeredSelectionShortcut_ =
        await this.browserProxy_.getGlicSelectionShortcut();
    this.hideHelpBubble(OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID);
  }

  // Records whether the shortcut enablement state transitioned from disabled to
  // enabled or vice versa.
  // TODO(crbug.com/406848612): Record these in the browser process instead.
  private recordShortcutEnablement() {
    if (this.shortcutInput_ && !this.removedShortcut_) {
      this.metricsBrowserProxy_.recordAction(
          'GlicOsEntrypoint.Settings.ShortcutEnabled');
    } else if (!this.shortcutInput_ && this.removedShortcut_) {
      this.metricsBrowserProxy_.recordAction(
          'GlicOsEntrypoint.Settings.ShortcutDisabled');
    } else {
      this.metricsBrowserProxy_.recordAction(
          'GlicOsEntrypoint.Settings.ShortcutEdited');
    }
  }

  protected onInputCaptureChange_(event: CustomEvent<boolean>) {
    const capturing = event.detail;
    this.browserProxy_.setShortcutSuspensionState(capturing);
    if (!capturing) {
      this.recordShortcutEnablement();
      this.removedShortcut_ = null;
    }
  }

  protected onTabAccessToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.TabContext' + (enabled ? '.Enabled' : '.Disabled'));
  }

  protected onTabAccessExpandClick_() {
    this.tabAccessToggleExpanded_ = !this.tabAccessToggleExpanded_;
  }

  protected onTabAccessToggleExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.tabAccessToggleExpanded_ = e.detail.value;
  }

  protected onDefaultTabAccessExpandClick_() {
    this.defaultTabAccessToggleExpanded_ =
        !this.defaultTabAccessToggleExpanded_;
  }

  protected onDefaultTabAccessToggleExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.defaultTabAccessToggleExpanded_ = e.detail.value;
  }

  protected onDefaultTabAccessToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.DefaultTabContext' +
        (enabled ? '.Enabled' : '.Disabled'));
  }

  protected onKeepSidepanelOpenOnNewTabsSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.KeepSidepanelOpenOnNewTabs' +
        (enabled ? '.Enabled' : '.Disabled'));
  }

  protected onShakeTriggerToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.ShakeTrigger' +
        (enabled ? '.Enabled' : '.Disabled'));
  }

  private onWebActuationEnabledChanged_(enabled: boolean) {
    if (this.isWebActuationDisabledForEnterprise_) {
      this.webActuationEnabledExpanded_ = false;
      return;
    }
    this.webActuationEnabledExpanded_ = enabled;
  }

  protected onActivityRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('glicActivityButtonUrl'));
  }

  protected onActorLoginPermissionsRowClick_() {
    Router.getInstance().navigateTo(routes.GEMINI_LOGIN);
  }

  protected onExtensionsRowClick_() {
    // TODO(crbug.com/434213151): Append url param when ready.
    const url = new URL(this.i18n('glicExtensionsManagementUrl'));
    OpenWindowProxyImpl.getInstance().openUrl(url.toString());
  }

  protected onShortcutsLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LEARN_MORE_CLICKED);
  }

  protected onLauncherToggleLearnMoreClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LAUNCHER_TOGGLE_LEARN_MORE_CLICKED);
  }

  protected onLocationToggleLearnMoreClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LOCATION_TOGGLE_LEARN_MORE_CLICKED);
  }

  protected onTabAccessToggleLearnMoreClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_TAB_ACCESS_TOGGLE_LEARN_MORE_CLICKED);
  }

  protected onTabAccessLearnMoreClick_() {
    this.onTabAccessToggleLearnMoreClicked_();
  }

  protected onDefaultTabAccessToggleSubLabelLinkClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions
            .GLIC_SHORTCUTS_DEFAULT_TAB_ACCESS_TOGGLE_LEARN_MORE_CLICKED);
    OpenWindowProxyImpl.getInstance().openUrl(
        this.getDefaultTabAccessLearnMoreUrl_());
  }

  protected onGeminiPersonalContextClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('geminiPersonalContextUrl'));
  }

  private disallowedByAdminChanged_(disallowed: boolean) {
    this.disallowedByAdmin_ = disallowed;
  }

  protected onClosedCaptionsToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.ClosedCaptions.' + (enabled ? 'Enabled' : 'Disabled'));
  }

  protected onTabstripButtonToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.TabstripButton.' + (enabled ? 'Enabled' : 'Disabled'));
  }

  protected isEnterpriseAccountDataProtected_(): boolean {
    return this.glicUserStatusCheckFeatureEnabled_ &&
        !!this.userStatusPref_?.value?.isEnterpriseAccountDataProtected;
  }

  protected getLocationSubLabel_(): string {
    return this.i18n(
        this.isEnterpriseAccountDataProtected_() ?
            'glicLocationToggleSublabelDataProtected' :
            'glicLocationToggleSublabel');
  }

  protected getLocationLearnMoreUrl_(): string {
    return this.isEnterpriseAccountDataProtected_() ?
        '' :
        this.i18n('glicLocationToggleLearnMoreUrl');
  }

  protected getMicrophoneSubLabel_(): string {
    return this.i18n(
        this.isEnterpriseAccountDataProtected_() ?
            'glicMicrophoneToggleSublabelDataProtected' :
            'glicMicrophoneToggleSublabel');
  }

  protected getTabAccessSubLabel_(): string {
    return this.i18n(
        this.isEnterpriseAccountDataProtected_() ?
            'glicTabAccessToggleSublabelDataProtected' :
            'glicTabAccessToggleSublabel');
  }

  protected getTabAccessLearnMoreUrl_(): string {
    return this.i18n(
        this.isEnterpriseAccountDataProtected_() ?
            'glicTabAccessToggleLearnMoreUrlDataProtected' :
            'glicTabAccessToggleLearnMoreUrl');
  }

  // i18nAdvanced is needed to allow for translating strings containing HTML.
  // The glicDefaultTabAccessToggleSublabel strings contain <ph> elements which
  // are translated to <a> tags to provide a link in the label.
  protected getDefaultTabAccessSubLabel_(): string {
    return this
        .i18nAdvanced(
            this.isEnterpriseAccountDataProtected_() ?
                'glicDefaultTabAccessToggleSublabelDataProtected' :
                'glicDefaultTabAccessToggleSublabel')
        .toString();
  }

  private getDefaultTabAccessLearnMoreUrl_(): string {
    return this.i18n(
        this.isEnterpriseAccountDataProtected_() ?
            'glicDefaultTabAccessToggleLearnMoreUrlDataProtected' :
            'glicDefaultTabAccessToggleLearnMoreUrl');
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    assert(childViewId === 'geminiLoginPermissions');
    const element = this.shadowRoot.querySelector<HTMLElement>(
        '#actorLoginPermissionsButton');
    assert(element);
    return element;
  }

  protected onWebActuationToggleSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.browserProxy_.setWebActuationEnabled(enabled);
    this.webActuationEnabledPref_ = {
      ...this.webActuationEnabledPref_,
      value: enabled,
    };
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.WebActuation' + (enabled ? '.Enabled' : '.Disabled'));
  }

  protected onWebActuationEnabledExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.webActuationEnabledExpanded_ = e.detail.value;
  }

  protected onExperimentalTriggeringSettingsBooleanControlChange_(
      event: CustomEvent<boolean>) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.browserProxy_.setExperimentalTriggeringEnabled(enabled);
    this.experimentalTriggeringEnabledPref_ = {
      ...this.experimentalTriggeringEnabledPref_,
      value: enabled,
    };
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.ExperimentalTriggering' +
        (enabled ? '.Enabled' : '.Disabled'));
  }

  private onExperimentalTriggeringEnabledChanged_(enabled: boolean) {
    this.experimentalTriggeringExpanded_ = enabled;
  }

  protected onExperimentalTriggeringExpandClick_() {
    this.experimentalTriggeringExpanded_ =
        !this.experimentalTriggeringExpanded_;
  }

  protected onExperimentalTriggeringExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.experimentalTriggeringExpanded_ = e.detail.value;
  }

  protected onExperimentalTriggeringToggleSubLabelLinkClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_WEB_ACTUATION_TOGGLE_LEARN_MORE_CLICKED);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('glicExperimentalTriggeringLearnMoreUrl'));
  }

  protected getExperimentalTriggeringSubLabel_(): string {
    return this
        .i18nAdvanced('glicExperimentalTriggeringSublabel', {
          attrs: ['aria-label', 'aria-description'],
        })
        .toString();
  }

  protected onWebActuationExpandClick_() {
    this.webActuationEnabledExpanded_ = !this.webActuationEnabledExpanded_;
  }

  protected onWebActuationToggleSubLabelLinkClicked_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_WEB_ACTUATION_TOGGLE_LEARN_MORE_CLICKED);
    OpenWindowProxyImpl.getInstance().openUrl(
        this.getWebActuationLearnMoreUrl_());
  }

  protected getWebActuationSubLabel_(): TrustedHTML {
    return this.i18nAdvanced('glicWebActuationToggleSublabelV2', {
      attrs: ['aria-label', 'aria-description', 'target'],
    });
  }

  protected getWebActuationToggleConsider_(): TrustedHTML {
    return this.i18nAdvanced('glicWebActuationToggleConsider2V2', {
      attrs: ['aria-label', 'aria-description', 'target'],
    });
  }

  private getWebActuationLearnMoreUrl_(): string {
    return loadTimeData.getString('glicWebActuationToggleLearnMoreUrl');
  }

  private onWebActuationCapabilityChanged_(canActOnWeb: boolean) {
    this.isWebActuationDisabledForEnterprise_ = !canActOnWeb;
    if (this.isWebActuationDisabledForEnterprise_) {
      this.webActuationEnabledExpanded_ = false;
    }
  }

  private onWebActuationToggleVisibilityChanged_(visible: boolean) {
    this.webActuationFeatureEnabled_ = visible;
  }

  protected isExperimentalTriggeringDisabled_(): boolean {
    return !this.webActuationEnabledPref_?.value ||
        this.isWebActuationDisabledForEnterprise_;
  }

  protected onMediaUnderstandingToggleSubLabelLinkClicked_() {
    // URL for "some websites" link.
    OpenWindowProxyImpl.getInstance().openUrl(
        'https://support.google.com/chrome?p=gic_media_questions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-subpage': SettingsGlicSubpageElement;
  }
}

customElements.define(
    SettingsGlicSubpageElement.is, SettingsGlicSubpageElement);

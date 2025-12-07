// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
import '../settings_page/settings_subpage.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AiPageActions} from '../ai_page/constants.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {GlicBrowserProxy} from './glic_browser_proxy.js';
import {GlicBrowserProxyImpl} from './glic_browser_proxy.js';
import {getTemplate} from './glic_subpage.html.js';

export enum SettingsGlicPageFeaturePrefName {
  CLOSED_CAPTIONS_ENABLED = 'glic.closed_captioning_enabled',
  GEOLOCATION_ENABLED = 'glic.geolocation_enabled',
  LAUNCHER_ENABLED = 'glic.launcher_enabled',
  MICROPHONE_ENABLED = 'glic.microphone_enabled',
  SETTINGS_POLICY = 'browser.gemini_settings',
  TAB_CONTEXT_ENABLED = 'glic.tab_context_enabled',
  TABSTRIP_BUTTON_ENABLED = 'glic.pinned_to_tabstrip',
  USER_STATUS = 'glic.user_status',
  DEFAULT_TAB_CONTEXT_ENABLED = 'glic.default_tab_context_enabled',
  WEB_ACTUATION_ENABLED = 'glic.user_enabled_actuation_on_web',
}

// browser_element_identifiers constants
const OS_WIDGET_TOGGLE_ELEMENT_ID = 'kGlicOsToggleElementId';
const OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID =
    'kGlicOsWidgetKeyboardShortcutElementId';

// Partial structure of the glic.user_status dictionary pref.
interface GlicUserStatusPref {
  isEnterpriseAccountDataProtected?: boolean;
}

const SettingsGlicSubpageElementBase = SettingsViewMixin(
    HelpBubbleMixin(I18nMixin(WebUiListenerMixin(PrefsMixin(PolymerElement)))));

export class SettingsGlicSubpageElement extends SettingsGlicSubpageElementBase {
  static get is() {
    return 'settings-glic-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disallowedByAdmin_: {
        type: Boolean,
        value: false,
      },

      registeredShortcut_: {
        type: String,
        value: '',
      },

      registeredFocusToggleShortcut_: {
        type: String,
        value: '',
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

      closedCaptionsFeatureEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('glicClosedCaptionsFeatureEnabled');
        },
      },

      glicExtensionsFeatureEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('glicExtensionsFeatureEnabled');
        },
      },

      glicUserStatusCheckFeatureEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('glicUserStatusCheckFeatureEnabled'),
      },

      showGlicDefaultTabContextSetting_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('showGlicDefaultTabContextSetting'),
      },

      showGlicPersonalContextLink_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showGeminiPersonalContextLink'),
      },

      locationSubLabel_: {
        type: String,
        computed: `computeLocationSubLabel_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      locationLearnMoreUrl_: {
        type: String,
        computed: `computeLocationLearnMoreUrl_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      microphoneSubLabel_: {
        type: String,
        computed: `computeMicrophoneSubLabel_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      tabAccessSubLabel_: {
        type: String,
        computed: `computeTabAccessSubLabel_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      tabAccessLearnMoreUrl_: {
        type: String,
        computed: `computeTabAccessLearnMoreUrl_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      defaultTabAccessToggleExpanded_: {
        type: Boolean,
        value: false,
      },

      defaultTabAccessSubLabel_: {
        type: String,
        computed: `computeDefaultTabAccessSubLabel_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      defaultTabAccessLearnMoreUrl_: {
        type: String,
        computed: `computeDefaultTabAccessLearnMoreUrl_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      spark_: {
        type: String,
        computed: `computeSpark_()`,
      },

      isEnterpriseAccountDataProtected_: {
        type: Boolean,
        computed: `computeIsEnterpriseAccountDataProtected_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      webActuationFeatureEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('glicWebActuationFeatureEnabled') &&
              loadTimeData.getBoolean('glicActorEnabled');
        },
      },

      isWebActuationDisabledForEnterprise_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isWebActuationDisabledForEnterprise');
        },
      },

      // Mock pref to show disabled toggle with enterprise policy indicator.
      webActuationDisabledForEnterprisePref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
            controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
          };
        },
      },

      webActuationEnabledExpanded_: {
        type: Boolean,
        value: false,
      },

      webActuationSubLabel_: {
        type: String,
        computed: `computeWebActuationSubLabel_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,
      },

      webActuationLearnMoreUrl_: {
        type: String,
        computed: `computeWebActuationLearnMoreUrl_(prefs.${
            SettingsGlicPageFeaturePrefName.USER_STATUS}.value)`,

      },
    };
  }

  static get observers() {
    return [
      'onTabContextEnabledChanged_(' +
          `prefs.${SettingsGlicPageFeaturePrefName.TAB_CONTEXT_ENABLED}.value)`,
      'onDefaultTabContextEnabledChanged_(' +
          `prefs.${
              SettingsGlicPageFeaturePrefName
                  .DEFAULT_TAB_CONTEXT_ENABLED}.value)`,
      'onWebActuationEnabledChanged_(' +
          `prefs.${
              SettingsGlicPageFeaturePrefName.WEB_ACTUATION_ENABLED}.value)`,

    ];
  }

  private shortcutInput_: string;
  private focusToggleShortcutInput_: string;
  private removedShortcut_: string|null = null;
  declare private disallowedByAdmin_: boolean;
  declare private registeredShortcut_: string;
  declare private registeredFocusToggleShortcut_: string;
  declare private fakePref_: chrome.settingsPrivate.PrefObject;
  private browserProxy_: GlicBrowserProxy = GlicBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  declare private tabAccessToggleExpanded_: boolean;
  declare private defaultTabAccessToggleExpanded_: boolean;
  declare private closedCaptionsFeatureEnabled_: boolean;
  declare private glicExtensionsFeatureEnabled_: boolean;
  declare private glicUserStatusCheckFeatureEnabled_: boolean;
  declare private showGlicDefaultTabContextSetting_: boolean;
  declare private showGlicPersonalContextLink_: boolean;
  declare private locationSubLabel_: string;
  declare private locationLearnMoreUrl_: string;
  declare private microphoneSubLabel_: string;
  declare private tabAccessSubLabel_: string;
  declare private tabAccessLearnMoreUrl_: string;
  declare private defaultTabAccessSubLabel_: string;
  declare private defaultTabAccessLearnMoreUrl_: string;
  declare private spark_: string;
  declare private isEnterpriseAccountDataProtected_: boolean;
  declare private webActuationSubLabel_: string;
  declare private webActuationLearnMoreUrl_: string;
  declare private webActuationFeatureEnabled_: boolean;
  declare private isWebActuationDisabledForEnterprise_: boolean;
  declare private webActuationDisabledForEnterprisePref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  declare private webActuationEnabledExpanded_: boolean;

  override async connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.getDisallowedByAdmin().then(
        this.disallowedByAdminChanged_.bind(this));
    this.addWebUiListener(
        'glic-disallowed-by-admin-changed',
        this.disallowedByAdminChanged_.bind(this));
    this.addWebUiListener(
        'glic-web-actuation-capability-changed',
        (canActOnWeb: boolean) =>
            this.onWebActuationCapabilityChanged_(canActOnWeb));
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
    this.registeredFocusToggleShortcut_ =
        await this.browserProxy_.getGlicFocusToggleShortcut();
    await CrSettingsPrefs.initialized;
  }

  private async onEnabledTemplateDomChange_() {
    await CrSettingsPrefs.initialized;
    if (this.disallowedByAdmin_) {
      return;
    }

    const launcherToggle =
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#launcherToggle');
    const shortcutInput =
        this.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#mainShortcutSetting .shortcut-input');
    assert(launcherToggle);
    assert(shortcutInput);

    this.registerHelpBubble(
        OS_WIDGET_TOGGLE_ELEMENT_ID, launcherToggle.getBubbleAnchor());
    this.registerHelpBubble(
        OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID, shortcutInput);
  }

  private onLauncherToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.browserProxy_.setGlicOsLauncherEnabled(enabled);
    this.metricsBrowserProxy_.recordAction(
        'Glic.OsEntrypoint.Settings.Toggle' +
        (enabled ? '.Enabled' : '.Disabled'));
    this.hideHelpBubble(OS_WIDGET_TOGGLE_ELEMENT_ID);
  }

  private onGeolocationToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.Geolocation' + (enabled ? '.Enabled' : '.Disabled'));
  }

  private onMicrophoneToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.Microphone' + (enabled ? '.Enabled' : '.Disabled'));
  }

  private async onShortcutUpdated_(event: CustomEvent<string>) {
    this.shortcutInput_ = event.detail;
    await this.browserProxy_.setGlicShortcut(this.shortcutInput_);
    if (this.removedShortcut_ === null) {
      this.removedShortcut_ = this.registeredShortcut_;
    }
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
    // Records true if the shortcut string is defined and not empty.
    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Glic.OsEntrypoint.Settings.Shortcut', !!this.shortcutInput_);
    this.hideHelpBubble(OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID);
  }

  private async onFocusToggleShortcutUpdated_(event: CustomEvent<string>) {
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

  private onInputCaptureChange_(event: CustomEvent<boolean>) {
    const capturing = event.detail;
    this.browserProxy_.setShortcutSuspensionState(capturing);
    if (!capturing) {
      this.recordShortcutEnablement();
      this.removedShortcut_ = null;
    }
  }

  // Update the tab access collapsible any time the tab access pref changes.
  private onTabContextEnabledChanged_(enabled: boolean) {
    this.tabAccessToggleExpanded_ = enabled;
  }

  private onDefaultTabContextEnabledChanged_(enabled: boolean) {
    this.defaultTabAccessToggleExpanded_ = enabled;
  }

  private onTabAccessToggleChange_(event: CustomEvent) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.TabContext' + (enabled ? '.Enabled' : '.Disabled'));
  }

  private onTabAccessExpand_() {
    this.tabAccessToggleExpanded_ = !this.tabAccessToggleExpanded_;
  }

  private onDefaultTabAccessExpand_() {
    this.defaultTabAccessToggleExpanded_ =
        !this.defaultTabAccessToggleExpanded_;
  }

  private onDefaultTabAccessToggleChange_(event: CustomEvent) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.DefaultTabContext' +
        (enabled ? '.Enabled' : '.Disabled'));
  }

  private onWebActuationEnabledChanged_(enabled: boolean) {
    if (this.isWebActuationDisabledForEnterprise_) {
      this.webActuationEnabledExpanded_ = false;
      return;
    }
    this.webActuationEnabledExpanded_ = enabled;
  }

  private onActivityRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('glicActivityButtonUrl'));
  }

  private onExtensionsRowClick_() {
    // TODO(crbug.com/434213151): Append url param when ready.
    const url = new URL(this.i18n('glicExtensionsManagementUrl'));
    OpenWindowProxyImpl.getInstance().openUrl(url.toString());
  }

  private onShortcutsLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LEARN_MORE_CLICKED);
  }

  private onLauncherToggleLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LAUNCHER_TOGGLE_LEARN_MORE_CLICKED);
  }

  private onLocationToggleLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_LOCATION_TOGGLE_LEARN_MORE_CLICKED);
  }

  private onTabAccessToggleLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_TAB_ACCESS_TOGGLE_LEARN_MORE_CLICKED);
  }

  private onDefaultTabAccessToggleLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions
            .GLIC_SHORTCUTS_DEFAULT_TAB_ACCESS_TOGGLE_LEARN_MORE_CLICKED);
    OpenWindowProxyImpl.getInstance().openUrl(
        this.defaultTabAccessLearnMoreUrl_);
  }

  private onGeminiPersonalContextClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('geminiPersonalContextUrl'));
  }

  private disallowedByAdminChanged_(disallowed: boolean) {
    this.disallowedByAdmin_ = disallowed;
  }

  private onClosedCaptionsToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.ClosedCaptions.' + (enabled ? 'Enabled' : 'Disabled'));
  }

  private onTabstripButtonToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.TabstripButton.' + (enabled ? 'Enabled' : 'Disabled'));
  }

  private computeIsEnterpriseAccountDataProtected_(
      userStatus: GlicUserStatusPref|undefined): boolean {
    return this.glicUserStatusCheckFeatureEnabled_ &&
        !!userStatus?.isEnterpriseAccountDataProtected;
  }

  private computeLocationSubLabel_(userStatus: GlicUserStatusPref|undefined):
      string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18n('glicLocationToggleSublabelDataProtected') :
        this.i18n('glicLocationToggleSublabel');
  }

  private computeLocationLearnMoreUrl_(
      userStatus: GlicUserStatusPref|undefined): string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        '' :
        this.i18n('glicLocationToggleLearnMoreUrl');
  }

  private computeMicrophoneSubLabel_(userStatus: GlicUserStatusPref|undefined):
      string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18n('glicMicrophoneToggleSublabelDataProtected') :
        this.i18n('glicMicrophoneToggleSublabel');
  }

  private computeTabAccessSubLabel_(userStatus: GlicUserStatusPref|undefined):
      string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18n('glicTabAccessToggleSublabelDataProtected') :
        this.i18n('glicTabAccessToggleSublabel');
  }

  private computeTabAccessLearnMoreUrl_(
      userStatus: GlicUserStatusPref|undefined): string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18n('glicTabAccessToggleLearnMoreUrlDataProtected') :
        this.i18n('glicTabAccessToggleLearnMoreUrl');
  }

  // i18nAdvanced is needed to allow for translating strings containing HTML.
  // The glicDefaultTabAccessToggleSublabel strings contain <ph> elements which
  // are translated to <a> tags to provide a link in the label.
  private computeDefaultTabAccessSubLabel_(
      userStatus: GlicUserStatusPref|undefined): string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18nAdvanced('glicDefaultTabAccessToggleSublabelDataProtected')
            .toString() :
        this.i18nAdvanced('glicDefaultTabAccessToggleSublabel').toString();
  }

  private computeDefaultTabAccessLearnMoreUrl_(
      userStatus: GlicUserStatusPref|undefined): string {
    return this.computeIsEnterpriseAccountDataProtected_(userStatus) ?
        this.i18n('glicDefaultTabAccessToggleLearnMoreUrlDataProtected') :
        this.i18n('glicDefaultTabAccessToggleLearnMoreUrl');
  }

  private computeSpark_() {
    return loadTimeData.getBoolean('glicAssetsV2Enabled') ?
        'settings-internal:sparkv2' :
        'settings-internal:spark';
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  private onWebActuationToggleChange_(event: CustomEvent) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.WebActuation' + (enabled ? '.Enabled' : '.Disabled'));
  }

  private onWebActuationExpand_() {
    this.webActuationEnabledExpanded_ = !this.webActuationEnabledExpanded_;
  }

  private onWebActuationToggleLearnMoreClick_() {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_SHORTCUTS_WEB_ACTUATION_TOGGLE_LEARN_MORE_CLICKED);
    OpenWindowProxyImpl.getInstance().openUrl(this.webActuationLearnMoreUrl_);
  }

  private computeWebActuationSubLabel_(): string {
    return this.i18nAdvanced('glicWebActuationToggleSublabel').toString();
  }

  private computeWebActuationLearnMoreUrl_(): string {
    return loadTimeData.getString('glicWebActuationToggleLearnMoreUrl');
  }

  private onWebActuationCapabilityChanged_(canActOnWeb: boolean) {
    this.isWebActuationDisabledForEnterprise_ = !canActOnWeb;
    if (this.isWebActuationDisabledForEnterprise_) {
      this.webActuationEnabledExpanded_ = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-subpage': SettingsGlicSubpageElement;
  }
}

customElements.define(
    SettingsGlicSubpageElement.is, SettingsGlicSubpageElement);

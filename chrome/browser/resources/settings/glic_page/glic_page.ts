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
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {GlicBrowserProxy} from './glic_browser_proxy.js';
import {GlicBrowserProxyImpl} from './glic_browser_proxy.js';
import {getTemplate} from './glic_page.html.js';

export enum SettingsGlicPageFeaturePrefName {
  CLOSED_CAPTIONS_ENABLED = 'glic.closed_captioning_enabled',
  GEOLOCATION_ENABLED = 'glic.geolocation_enabled',
  LAUNCHER_ENABLED = 'glic.launcher_enabled',
  MICROPHONE_ENABLED = 'glic.microphone_enabled',
  SETTINGS_POLICY = 'browser.gemini_settings',
  TAB_CONTEXT_ENABLED = 'glic.tab_context_enabled',
}

// browser_element_identifiers constants
const OS_WIDGET_TOGGLE_ELEMENT_ID = 'kGlicOsToggleElementId';
const OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID =
    'kGlicOsWidgetKeyboardShortcutElementId';

const SettingsGlicPageElementBase =
    HelpBubbleMixin(I18nMixin(WebUiListenerMixin(PrefsMixin(PolymerElement))));

export class SettingsGlicPageElement extends SettingsGlicPageElementBase {
  static get is() {
    return 'settings-glic-page';
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
    };
  }

  static get observers() {
    return [
      'onTabContextEnabledChanged_(' +
          `prefs.${SettingsGlicPageFeaturePrefName.TAB_CONTEXT_ENABLED}.value)`,
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
  declare private closedCaptionsFeatureEnabled_: boolean;

  override async connectedCallback() {
    super.connectedCallback();
    this.browserProxy_.getDisallowedByAdmin().then(
        this.disallowedByAdminChanged_.bind(this));
    this.addWebUiListener(
        'glic-disallowed-by-admin-changed',
        this.disallowedByAdminChanged_.bind(this));
    this.registeredShortcut_ = await this.browserProxy_.getGlicShortcut();
    this.registeredFocusToggleShortcut_ =
        await this.browserProxy_.getGlicFocusToggleShortcut();
    await CrSettingsPrefs.initialized;
  }

  private onGlicPageClick_() {
    Router.getInstance().navigateTo(routes.GEMINI);
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

  private onTabAccessToggleChange_(event: CustomEvent) {
    const target = event.target as SettingsToggleButtonElement;
    const enabled = target.checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.TabContext' + (enabled ? '.Enabled' : '.Disabled'));
  }

  private onActivityRowClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('glicActivityButtonUrl'));
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

  private onSettingsPageLearnMoreClick_(event: Event) {
    this.metricsBrowserProxy_.recordAction(
        AiPageActions.GLIC_COLLAPSED_LEARN_MORE_CLICKED);
    // Prevent navigation to the Glic page if only the learn more link was
    // clicked.
    event.stopPropagation();
  }

  private disallowedByAdminChanged_(disallowed: boolean) {
    this.disallowedByAdmin_ = disallowed;
  }

  private onClosedCaptionsToggleChange_(event: Event) {
    const enabled = (event.target as SettingsToggleButtonElement).checked;
    this.metricsBrowserProxy_.recordAction(
        'Glic.Settings.ClosedCaptions.' + (enabled ? 'Enabled' : 'Disabled'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-page': SettingsGlicPageElement;
  }
}

customElements.define(SettingsGlicPageElement.is, SettingsGlicPageElement);

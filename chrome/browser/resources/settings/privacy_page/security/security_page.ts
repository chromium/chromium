// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/collapse_radio_button.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import '../../settings_page/settings_subpage.js';
import '../../simple_confirmation_dialog.js';
import './secure_dns.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCollapseRadioButtonElement} from '../../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, SafeBrowsingInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {Router} from '../../router.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import {ContentSettingsTypes} from '../../site_settings/constants.js';
import type {SiteSettingsBrowserProxy} from '../../site_settings/site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from '../../site_settings/site_settings_browser_proxy.js';
import {isSettingEnabled} from '../../site_settings/site_settings_util.js';
import {HatsBrowserProxyImpl, SecurityPageInteraction} from '../hats_browser_proxy.js';

import {SafeBrowsingSetting} from './safe_browsing_types.js';
import {getCss} from './security_page.css.js';
import {getHtml} from './security_page.html.js';

/**
 * Enumeration of all HTTPS-First Mode setting states. Must be kept in sync with
 * the enum of the same name located in:
 * chrome/browser/ssl/https_first_mode_settings_tracker.h
 */
export enum HttpsFirstModeSetting {
  DISABLED = 0,
  // DEPRECATED: A separate Incognito setting never shipped.
  // ENABLED_INCOGNITO = 1,
  ENABLED_FULL = 2,
  ENABLED_BALANCED = 3,
}

export interface SettingsSecurityPageElement {
  $: {
    passwordsLeakToggle: SettingsToggleButtonElement,
    safeBrowsingDisabled: SettingsCollapseRadioButtonElement,
    safeBrowsingEnhanced: SettingsCollapseRadioButtonElement,
    safeBrowsingRadioGroup: SettingsRadioGroupElement,
    safeBrowsingReportingToggle: SettingsToggleButtonElement,
    safeBrowsingStandard: SettingsCollapseRadioButtonElement,
  };
}

function toSecurityPageInteraction(setting: SafeBrowsingSetting):
    SecurityPageInteraction {
  switch (setting) {
    case SafeBrowsingSetting.ENHANCED:
      return SecurityPageInteraction.RADIO_BUTTON_ENHANCED_CLICK;
    case SafeBrowsingSetting.STANDARD:
      return SecurityPageInteraction.RADIO_BUTTON_STANDARD_CLICK;
    case SafeBrowsingSetting.DISABLED:
      return SecurityPageInteraction.RADIO_BUTTON_DISABLE_CLICK;
    default:
      assertNotReachedCase(setting);
  }
}

const SettingsSecurityPageElementBase =
    HelpBubbleMixinLit(SettingsViewMixinLit(WebUiListenerMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement)))));

export type SecurityPageElement = SettingsSecurityPageElement;

export class SettingsSecurityPageElement extends
    SettingsSecurityPageElementBase {
  static get is() {
    return 'settings-security-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether the secure DNS setting should be displayed.
       */
      showSecureDnsSetting_: {type: Boolean},

      // <if expr="is_chromeos">
      /**
       * Whether a link to secure DNS OS setting should be displayed.
       */
      showSecureDnsSettingLink_: {type: Boolean},
      // </if>

      /**
       * Setting for HTTPS-First Mode when the toggle is off.
       */
      httpsFirstModeUncheckedValues_: {type: Array},

      javascriptOptimizerSubLabel_: {type: String},
      enableHttpsFirstModeNewSettings_: {type: Boolean},
      enableSecurityKeysSubpage_: {type: Boolean},
      enableHashPrefixRealTimeLookups_: {type: Boolean},
      hideExtendedReportingRadioButton_: {type: Boolean},
      showDisableSafebrowsingDialog_: {type: Boolean},

      /**
       * A timestamp that records the last time the user visited this page or
       * returned to it.
       */
      lastFocusTime_: {type: Number},

      /** The total amount of time a user spent on the page in focus. */
      totalTimeInFocus_: {type: Number},

      /** Latest user interaction type on the security page. */
      lastInteraction_: {type: Number},

      /** Safe browsing state when the page opened. */
      safeBrowsingStateOnOpen_: {type: Number},

      /** Whether the user is currently on the security page or not. */
      isRouteSecurity_: {type: Boolean},

      safeBrowsingPref_: {type: Object},
      httpsFirstModePref_: {type: Object},
      passwordLeakDetectionPref_: {type: Object},
      passwordManagerLeakDetectionPref_: {type: Object},
    };
  }

  protected accessor showSecureDnsSetting_: boolean =
      loadTimeData.getBoolean('showSecureDnsSetting');

  // <if expr="is_chromeos">
  protected accessor showSecureDnsSettingLink_: boolean =
      loadTimeData.getBoolean('showSecureDnsSettingLink');
  // </if>

  protected accessor enableSecurityKeysSubpage_: boolean =
      loadTimeData.getBoolean('enableSecurityKeysSubpage');
  protected accessor showDisableSafebrowsingDialog_: boolean = false;
  protected accessor enableHashPrefixRealTimeLookups_: boolean =
      loadTimeData.getBoolean('enableHashPrefixRealTimeLookups');
  protected accessor httpsFirstModeUncheckedValues_: HttpsFirstModeSetting[] =
      [HttpsFirstModeSetting.DISABLED];
  protected accessor enableHttpsFirstModeNewSettings_: boolean =
      loadTimeData.getBoolean('enableHttpsFirstModeNewSettings');
  protected accessor javascriptOptimizerSubLabel_: string = '';
  private accessor lastFocusTime_: number|undefined;
  private accessor totalTimeInFocus_: number = 0;
  private accessor lastInteraction_: SecurityPageInteraction =
      SecurityPageInteraction.NO_INTERACTION;
  private accessor safeBrowsingStateOnOpen_: SafeBrowsingSetting =
      SafeBrowsingSetting.STANDARD;
  private accessor isRouteSecurity_: boolean = true;
  private eventTracker_: EventTracker = new EventTracker();
  protected accessor hideExtendedReportingRadioButton_: boolean =
      loadTimeData.getBoolean('extendedReportingRemovePrefDependency') &&
      loadTimeData.getBoolean('hashPrefixRealTimeLookupsSamplePing');
  protected accessor safeBrowsingPref_:
      chrome.settingsPrivate.PrefObject<SafeBrowsingSetting>|undefined;
  protected accessor httpsFirstModePref_:
      chrome.settingsPrivate.PrefObject<HttpsFirstModeSetting>|undefined;
  protected accessor passwordLeakDetectionPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor passwordManagerLeakDetectionPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteBrowserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'generated.safe_browsing': 'safeBrowsingPref_',
      'generated.https_first_mode_enabled': 'httpsFirstModePref_',
      'generated.password_leak_detection': 'passwordLeakDetectionPref_',
      'profile.password_manager_leak_detection':
          'passwordManagerLeakDetectionPref_',
    });
  }

  override firstUpdated(changedProperties: PropertyValues) {
    super.firstUpdated(changedProperties);

    PrefService.getInstance().whenInitialized().then(() => {
      // Expand initial pref value manually because automatic
      // expanding is disabled.
      const prefValue =
          PrefService.getInstance()
              .getPref<SafeBrowsingSetting>('generated.safe_browsing')
              .value;
      if (prefValue === SafeBrowsingSetting.ENHANCED) {
        this.$.safeBrowsingEnhanced.expanded = true;
      } else if (prefValue === SafeBrowsingSetting.STANDARD) {
        this.$.safeBrowsingStandard.expanded = true;
      }

      this.safeBrowsingStateOnOpen_ = prefValue;

      // The HTTPS-First Mode generated pref should never be set to
      // ENABLED_BALANCED if the feature flag is not enabled.
      if (!loadTimeData.getBoolean('enableHttpsFirstModeNewSettings')) {
        assert(
            PrefService.getInstance()
                .getPref('generated.https_first_mode_enabled')
                .value !== HttpsFirstModeSetting.ENABLED_BALANCED);
      }
    });

    this.registerHelpBubble(
        'kEnhancedProtectionSettingElementId',
        this.$.safeBrowsingEnhanced.getBubbleAnchor(), {paddingTop: 10});

    // Initialize the last focus time on page load.
    this.lastFocusTime_ = HatsBrowserProxyImpl.getInstance().now();

    this.addWebUiListener(
        'contentSettingCategoryChanged', (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.JAVASCRIPT_OPTIMIZER) {
            this.updateJavascriptOptimizerEnabledByDefault_();
          }
        });
    this.updateJavascriptOptimizerEnabledByDefault_();
  }

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route, oldRoute?: Route) {
    super.currentRouteChanged(route, oldRoute);

    if (route !== routes.SECURITY) {
      // If the user navigates to other settings page from security page, call
      // onBeforeUnload_ method to check if the security page survey should be
      // shown.
      this.onBeforeUnload_();
      this.isRouteSecurity_ = false;
      this.eventTracker_.removeAll();
      return;
    }
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED);
    const queryParams = Router.getInstance().getQueryParameters();
    const section = queryParams.get('q');
    if (section === 'enhanced') {
      this.$.safeBrowsingEnhanced.expanded = false;
      this.$.safeBrowsingStandard.expanded = false;
    }

    this.eventTracker_.add(window, 'focus', this.onFocus_.bind(this));
    this.eventTracker_.add(window, 'blur', this.onBlur_.bind(this));
    this.eventTracker_.add(
        window, 'beforeunload', this.onBeforeUnload_.bind(this));

    // When the route changes back to the security page, reset the values.
    this.isRouteSecurity_ = true;
    this.lastInteraction_ = SecurityPageInteraction.NO_INTERACTION;
    this.totalTimeInFocus_ = 0;
    this.lastFocusTime_ = HatsBrowserProxyImpl.getInstance().now();
  }

  /** Call this function when the user switches to another tab. */
  private onBlur_() {
    // If the user is not on the security page, we will not calculate the time
    // values.
    if (!this.isRouteSecurity_) {
      return;
    }
    // Calculates the amount of time a user spent on a page for the current
    // session, from the point when they opened/returned to the page until
    // they left.
    const timeSinceLastFocus = HatsBrowserProxyImpl.getInstance().now() -
        (this.lastFocusTime_ as number);

    this.totalTimeInFocus_ += timeSinceLastFocus;
    // Set the lastFocusTime_ variable to undefined. This indicates that the
    // totalTimeInFocus_ variable is up to date.
    this.lastFocusTime_ = undefined;
  }

  /** Call this function when the user returns to it from other tabs. */
  private onFocus_() {
    // Updates the timestamp.
    this.lastFocusTime_ = HatsBrowserProxyImpl.getInstance().now();
  }

  /**
   * Trigger the securityPageHatsRequest api to potentially start the survey.
   */
  private onBeforeUnload_() {
    // If the user is not on other settings page, we do not send survey.
    if (!this.isRouteSecurity_) {
      return;
    }
    // If the lastFocusTime_ variable is not undefined, add the time between the
    // lastFocusTime_ and the current time to the totalTimeInFocus_ variable
    // because the user unloads the page before they un-focus on the page.
    if (this.lastFocusTime_ !== undefined) {
      this.totalTimeInFocus_ +=
          HatsBrowserProxyImpl.getInstance().now() - this.lastFocusTime_;
    }
    HatsBrowserProxyImpl.getInstance().securityPageHatsRequest(
        this.lastInteraction_, this.safeBrowsingStateOnOpen_,
        this.totalTimeInFocus_);
  }

  /**
   * Updates the buttons' expanded status by propagating previous click
   * events
   */
  private updateCollapsedButtons_() {
    this.$.safeBrowsingEnhanced.updateCollapsed();
    this.$.safeBrowsingStandard.updateCollapsed();
  }

  private async updateJavascriptOptimizerEnabledByDefault_() {
    const defaultValue =
        await this.siteBrowserProxy_.getDefaultValueForContentType(
            ContentSettingsTypes.JAVASCRIPT_OPTIMIZER);
    this.javascriptOptimizerSubLabel_ = this.i18n(
        isSettingEnabled(defaultValue.setting) ?
            'securityJavascriptOptimizerLinkRowLabelEnabled' :
            'securityJavascriptOptimizerLinkRowLabelDisabled');
  }

  /**
   * Possibly displays the Safe Browsing disable dialog based on the users
   * selection.
   */
  protected onSafeBrowsingRadioChange_() {
    const selected =
        Number.parseInt(this.$.safeBrowsingRadioGroup.selected || '', 10);
    const prefValue =
        PrefService.getInstance().getPref('generated.safe_browsing').value;
    if (prefValue !== selected) {
      this.recordInteractionHistogramOnRadioChange_(selected);
      this.recordActionOnRadioChange_(selected);
      this.interactedWithPage_(toSecurityPageInteraction(selected));
    }
    if (selected === SafeBrowsingSetting.DISABLED) {
      this.showDisableSafebrowsingDialog_ = true;
    } else {
      this.updateCollapsedButtons_();
      this.$.safeBrowsingRadioGroup.sendPrefChange();
    }
  }

  private interactedWithPage_(securityPageInteraction:
                                  SecurityPageInteraction) {
    this.lastInteraction_ = securityPageInteraction;
  }

  protected getDisabledExtendedSafeBrowsing_(): boolean {
    return this.safeBrowsingPref_?.value !== SafeBrowsingSetting.STANDARD;
  }

  protected getSafeBrowsingStandardSubLabel_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'safeBrowsingStandardDescProxy' :
            'safeBrowsingStandardDesc');
  }

  protected getPasswordsLeakToggleSubLabel_(): string {
    let subLabel = this.i18n('passwordsLeakDetectionGeneralDescription');
    // If the backing password leak detection preference is enabled, but the
    // generated preference is off and user control is disabled, then additional
    // text explaining that the feature will be enabled if the user signs in is
    // added.
    if (this.passwordLeakDetectionPref_ &&
        this.passwordManagerLeakDetectionPref_) {
      const generatedPref = this.passwordLeakDetectionPref_;
      if (this.passwordManagerLeakDetectionPref_.value &&
          !generatedPref.value && generatedPref.userControlDisabled) {
        subLabel +=
            ' ' +  // Whitespace is a valid sentence separator w.r.t. i18n.
            this.i18n('passwordsLeakDetectionSignedOutEnabledDescription');
      }
    }
    return subLabel;
  }

  // Conversion helper for binding Integer pref values as String values.
  // For ControlledRadioButton elements, the name attribute must be of String
  // type in order to correctly match for the PrefControlMixin.
  protected getName_(value: number): string {
    return value.toString();
  }

  protected getHttpsFirstModeSubLabel_(): string {
    // If the backing HTTPS-Only Mode preference is enabled, but the
    // generated preference has its user control disabled, then additional
    // text explaining that the feature is locked down for Advanced Protection
    // users is added.
    const generatedPref = this.httpsFirstModePref_;
    if (!generatedPref) {
      return '';
    }
    if (this.enableHttpsFirstModeNewSettings_) {
      return this.i18n(
          generatedPref.userControlDisabled ?
              'httpsFirstModeDescriptionAdvancedProtection' :
              'httpsFirstModeSectionDescription');
    } else {
      return this.i18n(
          generatedPref.userControlDisabled ?
              'httpsOnlyModeDescriptionAdvancedProtection' :
              'httpsOnlyModeDescription');
    }
  }

  protected isHttpsFirstModeExpanded_(): boolean {
    // If the pref is not user-modifiable, we should only show the main toggle.
    // (Note: this is not the case when the setting is policy-managed -- the
    // radio group should be expanded and labeled with the enterprise
    // indicator.)
    const generatedPref = this.httpsFirstModePref_;
    if (!generatedPref || generatedPref.userControlDisabled) {
      return false;
    }
    return generatedPref.value !== HttpsFirstModeSetting.DISABLED;
  }

  protected onManageCertificatesClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('certManagementV2URL'));
  }

  protected onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  }

  protected onJavascriptOptimizerSettingsClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER);
  }

  protected onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  }

  protected onEnhancedProtectionLearnMoreClick_(e: Event) {
    if ((e.target as HTMLElement).id === 'enhancedProtectionLearnMoreLink') {
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('enhancedProtectionHelpCenterURL'));
      e.preventDefault();
    }
  }

  protected onSafeBrowsingExtendedReportingChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IMPROVE_SECURITY);
  }

  /**
   * Handles the closure of the disable safebrowsing dialog, reselects the
   * appropriate radio button if the user cancels the dialog, and puts focus on
   * the disable safebrowsing button.
   */
  protected onDisableSafebrowsingDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    const confirmed = dialog.wasConfirmed();
    this.recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed);
    this.recordActionOnSafeBrowsingDialogClose_(confirmed);
    // Check if the dialog was confirmed before closing it.
    if (confirmed) {
      this.$.safeBrowsingRadioGroup.sendPrefChange();
      this.updateCollapsedButtons_();
    } else {
      this.$.safeBrowsingRadioGroup.resetToPrefValue();
    }

    this.showDisableSafebrowsingDialog_ = false;

    // Set focus back to the no protection button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    focusWithoutInk(this.$.safeBrowsingDisabled);
  }

  protected onEnhancedProtectionExpandClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.ENHANCED);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.ENHANCED);
    this.interactedWithPage_(
        SecurityPageInteraction.EXPAND_BUTTON_ENHANCED_CLICK);
  }

  protected onStandardProtectionExpandClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.STANDARD);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.STANDARD);
    this.interactedWithPage_(
        SecurityPageInteraction.EXPAND_BUTTON_STANDARD_CLICK);
  }

  // <if expr="is_chromeos">
  protected onOpenChromeOsSecureDnsSettingsClick_() {
    const path =
        loadTimeData.getString('chromeOSPrivacyAndSecuritySectionPath');
    OpenWindowProxyImpl.getInstance().openUrl(`chrome://os-settings/${path}`);
  }
  // </if>

  private recordInteractionHistogramOnRadioChange_(safeBrowsingSetting:
                                                       SafeBrowsingSetting) {
    let action;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED;
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_STANDARD_PROTECTION_CLICKED;
    } else {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED;
    }
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(action);
  }

  private recordInteractionHistogramOnExpandButtonClicked_(
      safeBrowsingSetting: SafeBrowsingSetting) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            SafeBrowsingInteractions
                .SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED :
            SafeBrowsingInteractions
                .SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED);
  }

  private recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed:
                                                                   boolean) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        confirmed ? SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED :
                    SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED);
  }

  private recordActionOnRadioChange_(safeBrowsingSetting: SafeBrowsingSetting) {
    let actionName;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      actionName = 'SafeBrowsing.Settings.EnhancedProtectionClicked';
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      actionName = 'SafeBrowsing.Settings.StandardProtectionClicked';
    } else {
      actionName = 'SafeBrowsing.Settings.DisableSafeBrowsingClicked';
    }
    this.metricsBrowserProxy_.recordAction(actionName);
  }

  private recordActionOnExpandButtonClicked_(safeBrowsingSetting:
                                                 SafeBrowsingSetting) {
    this.metricsBrowserProxy_.recordAction(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            'SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked' :
            'SafeBrowsing.Settings.StandardProtectionExpandArrowClicked');
  }

  private recordActionOnSafeBrowsingDialogClose_(confirmed: boolean) {
    this.metricsBrowserProxy_.recordAction(
        confirmed ? 'SafeBrowsing.Settings.DisableSafeBrowsingDialogConfirmed' :
                    'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied');
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map([
      [
        routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER.path,
        '#javascriptOptimizerSettingLink',
      ],
    ]);

    if (routes.SECURITY_KEYS) {
      map.set(routes.SECURITY_KEYS.path, '#securityKeysSubpageTrigger');
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page': SettingsSecurityPageElement;
  }
}

customElements.define(
    SettingsSecurityPageElement.is, SettingsSecurityPageElement);

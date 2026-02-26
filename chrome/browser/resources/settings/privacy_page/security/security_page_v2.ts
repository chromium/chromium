// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../icons.html.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_section.js';
import '../../settings_page/settings_subpage.js';
import './security_page_feature_row.js';
import './secure_dns.js';
import './secure_dns_v2.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import {SecureDnsMode} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixin, Router} from '../../router.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import {JavascriptOptimizerSetting} from '../../site_settings/constants.js';
import type {HatsBrowserProxy} from '../hats_browser_proxy.js';
import {HatsBrowserProxyImpl, SecurityPageV2Interaction} from '../hats_browser_proxy.js';

import {SafeBrowsingSetting} from './safe_browsing_types.js';
import {SecureDnsV2ResolverType} from './secure_dns_v2.js';
import type {SettingsSecureDnsV2Element} from './secure_dns_v2.js';
import type {SecurityPageFeatureRowElement} from './security_page_feature_row.js';
import {getTemplate} from './security_page_v2.html.js';

/** Enumeration of all security settings bundle modes.*/
// LINT.IfChange(SecuritySettingsBundleSetting)
export enum SecuritySettingsBundleSetting {
  STANDARD = 0,
  ENHANCED = 1,
}
// LINT.ThenChange(/components/safe_browsing/core/common/safe_browsing_prefs.h:SecuritySettingsBundleSetting)

/** Enumeration of all HTTPS-First Mode setting states.*/
// LINT.IfChange(HttpsFirstModeSetting)
export enum HttpsFirstModeSetting {
  DISABLED = 0,
  // DEPRECATED: A separate Incognito setting never shipped.
  // ENABLED_INCOGNITO = 1,
  ENABLED_FULL = 2,
  ENABLED_BALANCED = 3,
}
// LINT.ThenChange(/chrome/browser/ssl/https_first_mode_settings_tracker.h)

export interface SettingsSecurityPageV2Element {
  $: {
    blockForAllSites: ControlledRadioButtonElement,
    blockForUnfamiliarSites: ControlledRadioButtonElement,
    bundlesRadioGroup: SettingsRadioGroupElement,
    httpsFirstModeEnabledBalanced: ControlledRadioButtonElement,
    httpsFirstModeEnabledStrict: ControlledRadioButtonElement,
    httpsFirstModeRadioGroup: SettingsRadioGroupElement,
    httpsFirstModeToggle: SettingsToggleButtonElement,
    javascriptGuardrailsRow: SecurityPageFeatureRowElement,
    manageSiteExceptionsButton: CrButtonElement,
    passwordsLeakToggle: SettingsToggleButtonElement,
    resetEnhancedBundleToDefaultsButton: CrButtonElement,
    resetStandardBundleToDefaultsButton: CrButtonElement,
    securitySettingsBundleEnhanced: ControlledRadioButtonElement,
    securitySettingsBundleStandard: ControlledRadioButtonElement,
    safeBrowsingRadioGroup: SettingsRadioGroupElement,
    safeBrowsingRow: SecurityPageFeatureRowElement,
    secureDnsV2Row: SettingsSecureDnsV2Element,
  };
}

const SettingsSecurityPageV2ElementBase = RouteObserverMixin(
    SettingsViewMixin(WebUiListenerMixin(PrefsMixin(PolymerElement))));

export class SettingsSecurityPageV2Element extends
    SettingsSecurityPageV2ElementBase {
  static get is() {
    return 'settings-security-page-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      securitySettingsBundleSettingEnum_: {
        type: Object,
        value: SecuritySettingsBundleSetting,
      },

      safeBrowsingSettingEnum_: {
        type: Object,
        value: SafeBrowsingSetting,
      },

      httpsFirstModeSettingEnum_: {
        type: Object,
        value: HttpsFirstModeSetting,
      },

      isResetStandardBundleToDefaultsButtonVisible_: {
        type: Boolean,
        value: false,
      },

      isResetEnhancedBundleToDefaultsButtonVisible_: {
        type: Boolean,
        value: false,
      },

      // Whether the security-setting-bundle is being reset to default.
      isResettingToDefaults_: {
        type: Boolean,
        value: false,
      },

      isHttpsFirstModeEnabled_: {
        type: Boolean,
        value: true,
      },

      isSafeBrowsingEnabled_: {
        type: Boolean,
        value: true,
      },

      isSafeBrowsingWarningIconVisible_: {
        type: Boolean,
        value: false,
      },

      safeBrowsingOff_: {
        type: Array,
        value: () => [SafeBrowsingSetting.DISABLED],
      },

      httpsFirstModeUncheckedValues_: {
        type: Array,
        value: () => [HttpsFirstModeSetting.DISABLED],
      },

      javascriptGuardrailsOff_: {
        type: Array,
        value: () => [JavascriptOptimizerSetting.ALLOWED],
      },

      safeBrowsingStateTextMap_: {
        type: Object,
        value: () => ({
          [SafeBrowsingSetting.ENHANCED]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [SafeBrowsingSetting.STANDARD]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
          [SafeBrowsingSetting.DISABLED]:
              loadTimeData.getString('securityFeatureRowStateOff'),
        }),
      },

      javascriptGuardrailsStateTextMap_: {
        type: Object,
        value: () => ({
          [JavascriptOptimizerSetting.BLOCKED_FOR_UNFAMILIAR_SITES]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [JavascriptOptimizerSetting.ALLOWED]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
          [JavascriptOptimizerSetting.BLOCKED]:
              loadTimeData.getString('securityFeatureRowStateEnhancedStrict'),
        }),
      },

      enableSecurityKeysSubpage_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysSubpage');
        },
      },

      enableBundledSecuritySettingsSecureDnsV2_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'),
      },

      javascriptOptimizerSettingEnum_: {
        type: Object,
        value: JavascriptOptimizerSetting,
      },

      shouldHideBundles_: {
        type: Boolean,
        computed: 'computeShouldHideBundles_(' +
            'prefs.generated.safe_browsing.*, ' +
            'prefs.dns_over_https.mode.*, ' +
            'prefs.generated.javascript_optimizer.*, ' +
            'enableBundledSecuritySettingsSecureDnsV2_)',
      },
    };
  }

  static get observers() {
    return [
      'updateResetButtonVisibility_(' +
          'isResettingToDefaults_,' +
          'prefs.generated.security_settings_bundle.value,' +
          'prefs.generated.safe_browsing.*,' +
          'prefs.dns_over_https.mode.*, ' +
          'prefs.dns_over_https.templates.*, ' +
          'prefs.dns_over_https.automatic_mode_fallback_to_doh.*,' +
          'prefs.generated.javascript_optimizer.*),',
      'updateRowsState_(' +
          'prefs.generated.https_first_mode_enabled.*,' +
          'prefs.generated.safe_browsing.*,' +
          'prefs.generated.javascript_optimizer.*),',
    ];
  }

  // Keep in alphabetical order.
  declare private enableBundledSecuritySettingsSecureDnsV2_: boolean;
  declare private enableSecurityKeysSubpage_: boolean;
  declare private httpsFirstModeUncheckedValues_: HttpsFirstModeSetting[];
  declare private isResettingToDefaults_: boolean;
  declare private isResetStandardBundleToDefaultsButtonVisible_: boolean;
  declare private isResetEnhancedBundleToDefaultsButtonVisible_: boolean;
  declare private isHttpsFirstModeEnabled_: boolean;
  declare private isSafeBrowsingEnabled_: boolean;
  declare private isSafeBrowsingWarningIconVisible_: boolean;
  declare private javascriptGuardrailsOff_: JavascriptOptimizerSetting[];
  declare private javascriptGuardrailsStateTextMap_: Object;
  declare private safeBrowsingOff_: SafeBrowsingSetting[];
  declare private safeBrowsingStateTextMap_: Object;
  declare private shouldHideBundles_: boolean;

  private lastFocusTime_: number|undefined;
  private totalTimeInFocus_: number = 0;
  private interactions_: Set<SecurityPageV2Interaction> = new Set();
  private safeBrowsingStateOnOpen_: SafeBrowsingSetting;
  private securitySettingsBundleStateOnOpen_: SecuritySettingsBundleSetting;
  private isRouteSecurity_: boolean = true;
  private eventTracker_: EventTracker = new EventTracker();
  private hatsBrowserProxy_: HatsBrowserProxy =
      HatsBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    // Initialize the last focus time on page load.
    this.lastFocusTime_ = this.hatsBrowserProxy_.now();
  }

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route) {
    if (route !== routes.SECURITY) {
      this.onBeforeUnload_();
      this.isRouteSecurity_ = false;
      this.eventTracker_.removeAll();
      return;
    }
    this.eventTracker_.add(window, 'focus', this.onFocus_.bind(this));
    this.eventTracker_.add(window, 'blur', this.onBlur_.bind(this));
    this.eventTracker_.add(
        window, 'beforeunload', this.onBeforeUnload_.bind(this));

    // When the route changes to the security page, reset the state.
    // This is done here instead of in ready() because the user may navigate to
    // another settings page in the same tab and then come back to this
    // security page. These would be treated as two different sessions.
    this.isRouteSecurity_ = true;
    this.interactions_.clear();
    this.totalTimeInFocus_ = 0;
    this.lastFocusTime_ = this.hatsBrowserProxy_.now();
    CrSettingsPrefs.initialized.then(() => {
      this.safeBrowsingStateOnOpen_ =
          this.getPref('generated.safe_browsing').value;
      this.securitySettingsBundleStateOnOpen_ =
          this.getPref('generated.security_settings_bundle').value;
    });
  }

  /** Updates the total time in focus when the page loses focus. */
  private onBlur_() {
    // If the user is not on the security page, we will not add to the total
    // time in focus.
    if (!this.isRouteSecurity_) {
      return;
    }
    // Add the amount of time a user spent on the page for the
    // current session to the total time in focus.
    const timeSinceLastFocus =
        this.hatsBrowserProxy_.now() - (this.lastFocusTime_ as number);
    this.totalTimeInFocus_ += timeSinceLastFocus;

    // Set the lastFocusTime_ variable to undefined. This indicates that the
    // totalTimeInFocus_ variable is up to date.
    this.lastFocusTime_ = undefined;
  }

  /**
   * Updates the timestamp when the user returns to this security page from
   * another tab.
   */
  private onFocus_() {
    this.lastFocusTime_ = this.hatsBrowserProxy_.now();
  }

  /**
   * Trigger the securityPageHatsRequest api to potentially start the survey.
   */
  private onBeforeUnload_() {
    if (!this.isRouteSecurity_) {
      return;
    }
    if (this.safeBrowsingStateOnOpen_ === undefined ||
        this.securitySettingsBundleStateOnOpen_ === undefined) {
      return;
    }
    // Calculate the total time spent on the page.
    if (this.lastFocusTime_ !== undefined) {
      this.totalTimeInFocus_ +=
          this.hatsBrowserProxy_.now() - this.lastFocusTime_;
    }

    const interactions = Array.from(this.interactions_);
    this.hatsBrowserProxy_.securityPageHatsRequest(
        interactions, this.safeBrowsingStateOnOpen_, this.totalTimeInFocus_,
        this.securitySettingsBundleStateOnOpen_);
  }

  /**
   * Handles clicks on the safe browsing row.
   */
  private onSafeBrowsingRowClick_(e: CustomEvent<{value: boolean}>) {
    // Contains the new state of the row (true if expanded, false if collapsed).
    const isExpanded = e.detail.value;
    if (isExpanded) {
      this.interactions_.add(
          SecurityPageV2Interaction.SAFE_BROWSING_ROW_EXPANDED);
      this.metricsBrowserProxy_.recordAction(
          'SafeBrowsing.Settings.SafeBrowsingRowExpanded');
    }
  }

  private onSafeBrowsingToggleChange_() {
    this.interactions_.add(
        SecurityPageV2Interaction.SAFE_BROWSING_TOGGLE_CLICK);

    if (!this.isSafeBrowsingEnabled_) {
      this.metricsBrowserProxy_.recordAction(
          'SafeBrowsing.Settings.DisableSafeBrowsingClicked');
    }
  }

  /**
   * Handles changes of the radio button selection inside the safe browsing
   * settings row.
   */
  private onSafeBrowsingRadioChange_() {
    const selected =
        Number.parseInt(this.$.safeBrowsingRadioGroup.selected || '', 10);
    if (selected === SafeBrowsingSetting.STANDARD) {
      this.interactions_.add(
          SecurityPageV2Interaction.STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK);
      this.metricsBrowserProxy_.recordAction(
          'SafeBrowsing.Settings.StandardProtectionClicked');
    } else if (selected === SafeBrowsingSetting.ENHANCED) {
      this.interactions_.add(
          SecurityPageV2Interaction.ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK);
      this.metricsBrowserProxy_.recordAction(
          'SafeBrowsing.Settings.EnhancedProtectionClicked');
    }
  }

  private onSecureDnsRowExpandedChange_(e: CustomEvent<{value: boolean}>) {
    // Contains the new state of the row (true if expanded, false if collapsed).
    const isExpanded = e.detail.value;
    if (isExpanded) {
      this.interactions_.add(
          SecurityPageV2Interaction.SECURE_DNS_V2_ROW_EXPANDED);
    }
  }

  private onSecureDnsToggleChange_() {
    this.interactions_.add(
        SecurityPageV2Interaction.SECURE_DNS_V2_TOGGLE_CLICK);
  }

  private onSecureDnsToggleClick_() {
    this.interactions_.add(SecurityPageV2Interaction.SECURE_DNS_TOGGLE_CLICK);
  }

  /**
   * Handles changes of the radio button selection inside the secure DNS
   * settings row.
   */
  private onSecureDnsRadioGroupChange_(e: CustomEvent<{value: string}>) {
    const selected = e.detail.value as SecureDnsV2ResolverType;

    switch (selected) {
      case SecureDnsV2ResolverType.AUTOMATIC:
        this.interactions_.add(SecurityPageV2Interaction
                                   .SECURE_DNS_V2_AUTOMATIC_RADIO_BUTTON_CLICK);
        break;
      case SecureDnsV2ResolverType.FALLBACK:
        this.interactions_.add(SecurityPageV2Interaction
                                   .SECURE_DNS_V2_FALLBACK_RADIO_BUTTON_CLICK);
        break;
      case SecureDnsV2ResolverType.CUSTOM:
        this.interactions_.add(
            SecurityPageV2Interaction.SECURE_DNS_V2_CUSTOM_RADIO_BUTTON_CLICK);
        break;
      case SecureDnsV2ResolverType.BUILT_IN:
        // There is technically no button for the built-in resolver type.
        break;
      default:
        assertNotReachedCase(
            selected,
            'Received unknown secure DNS radio button selection: ' + selected);
    }
  }

  private onHttpsFirstModeToggleChange_() {
    this.interactions_.add(
        SecurityPageV2Interaction.HTTPS_FIRST_MODE_TOGGLE_CLICK);
  }

  private onHttpsFirstModeRadioGroupChange_() {
    const selected =
        Number.parseInt(this.$.httpsFirstModeRadioGroup.selected || '', 10);
    if (selected === HttpsFirstModeSetting.ENABLED_BALANCED) {
      this.interactions_.add(SecurityPageV2Interaction
                                 .BALANCED_HTTPS_FIRST_MODE_RADIO_BUTTON_CLICK);
    } else if (selected === HttpsFirstModeSetting.ENABLED_FULL) {
      this.interactions_.add(
          SecurityPageV2Interaction.STRICT_HTTPS_FIRST_MODE_RADIO_BUTTON_CLICK);
    }
  }

  private onPasswordsLeakToggleChange_() {
    this.interactions_.add(
        SecurityPageV2Interaction.PASSWORD_LEAK_DETECTION_TOGGLE_CLICK);
  }

  private onManageCertificatesClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('certManagementV2URL'));
  }

  private onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  }

  private onAdvancedProtectionProgramClick_(e: Event) {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('advancedProtectionURL'));
    // The Advanced Protection Program link is part of a string that is
    // contained in a link-row. The default link navigation action will
    // be ignored to ensure that the click will only be registered for
    // the link inside the string and not also for the link-row.
    if ((e.target as HTMLElement).tagName === 'A') {
      e.preventDefault();
      return;
    }
  }

  private onManageSiteExceptionsClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER);
  }

  private onSecurityBundleChanged_() {
    const selected =
        Number.parseInt(this.$.bundlesRadioGroup.selected || '', 10);
    if (selected === SecuritySettingsBundleSetting.STANDARD) {
      this.interactions_.add(
          SecurityPageV2Interaction.STANDARD_BUNDLE_RADIO_BUTTON_CLICK);
    } else if (selected === SecuritySettingsBundleSetting.ENHANCED) {
      this.interactions_.add(
          SecurityPageV2Interaction.ENHANCED_BUNDLE_RADIO_BUTTON_CLICK);
    }

    this.resetBundleToDefaults_();
  }

  private onResetBundleToDefaultsButtonClick_() {
    this.resetBundleToDefaults_();
  }

  private resetBundleToDefaults_() {
    this.isResettingToDefaults_ = true;
    const bundleSetting = this.getBundleSetting_();
    this.setPrefValue(
        'generated.safe_browsing',
        this.getDefaultSafeBrowsingValue_(bundleSetting));
    this.setPrefValue(
        'generated.javascript_optimizer',
        this.getDefaultJsGuardrailsValue_(bundleSetting));
    if (this.enableBundledSecuritySettingsSecureDnsV2_) {
      this.setPrefValue(
          'dns_over_https.mode', this.getDefaultSecureDnsModeValue_());
      this.setPrefValue(
          'dns_over_https.templates',
          this.getDefaultSecureDnsTemplatesValue_());
      this.setPrefValue(
          'dns_over_https.automatic_mode_fallback_to_doh',
          this.getDefaultSecureDnsFallbackValue_(bundleSetting));
    }
    this.isResettingToDefaults_ = false;
  }

  private getBundleSetting_() {
    return this.getPref('generated.security_settings_bundle').value;
  }

  private getDefaultSafeBrowsingValue_(
      bundleSetting: SecuritySettingsBundleSetting) {
    return loadTimeData.getInteger(
        (bundleSetting === SecuritySettingsBundleSetting.ENHANCED) ?
            'securityEnhancedBundleSafeBrowsingDefault' :
            'securityStandardBundleSafeBrowsingDefault');
  }

  private getDefaultJsGuardrailsValue_(
      bundleSetting: SecuritySettingsBundleSetting) {
    return loadTimeData.getInteger(
        (bundleSetting === SecuritySettingsBundleSetting.ENHANCED) ?
            'securityEnhancedBundleJavascriptGuardrailsDefault' :
            'securityStandardBundleJavascriptGuardrailsDefault');
  }

  private getDefaultSecureDnsModeValue_() {
    if (!this.enableBundledSecuritySettingsSecureDnsV2_) {
      return null;
    }

    return SecureDnsMode.AUTOMATIC;
  }

  private getDefaultSecureDnsTemplatesValue_() {
    if (!this.enableBundledSecuritySettingsSecureDnsV2_) {
      return null;
    }

    return '';
  }

  private getDefaultSecureDnsFallbackValue_(
      bundleSetting: SecuritySettingsBundleSetting) {
    if (!this.enableBundledSecuritySettingsSecureDnsV2_) {
      return null;
    }

    return (bundleSetting === SecuritySettingsBundleSetting.ENHANCED);
  }

  private updateResetButtonVisibility_() {
    this.isResetStandardBundleToDefaultsButtonVisible_ = false;
    this.isResetEnhancedBundleToDefaultsButtonVisible_ = false;

    if (this.isResettingToDefaults_) {
      return;
    }

    const bundleSetting = this.getBundleSetting_();

    // LINT.IfChange
    const prefsToCheck = [
      {
        prefKey: 'generated.safe_browsing',
        defaultValue: this.getDefaultSafeBrowsingValue_(bundleSetting),
      },
      {
        prefKey: 'dns_over_https.mode',
        defaultValue: this.getDefaultSecureDnsModeValue_(),
      },
      {
        prefKey: 'dns_over_https.templates',
        defaultValue: this.getDefaultSecureDnsTemplatesValue_(),
      },
      {
        prefKey: 'dns_over_https.automatic_mode_fallback_to_doh',
        defaultValue: this.getDefaultSecureDnsFallbackValue_(bundleSetting),
      },
      {
        prefKey: 'generated.javascript_optimizer',
        defaultValue: this.getDefaultJsGuardrailsValue_(bundleSetting),
      },
    ];
    // LINT.ThenChange(//chrome/browser/safe_browsing/safe_browsing_service.cc,//chrome/browser/safe_browsing/metrics/bundled_settings_metrics_provider.cc)
    for (const prefToCheck of prefsToCheck) {
      const pref = this.getPref(prefToCheck.prefKey);
      if (prefToCheck.defaultValue != null &&
          pref.value !== prefToCheck.defaultValue &&
          pref.controlledBy == null) {
        if (bundleSetting === SecuritySettingsBundleSetting.ENHANCED) {
          this.isResetEnhancedBundleToDefaultsButtonVisible_ = true;
        } else {
          this.isResetStandardBundleToDefaultsButtonVisible_ = true;
        }
        return;
      }
    }
  }

  private updateRowsState_() {
    const httpsFirstModePref =
        this.getPref('generated.https_first_mode_enabled');
    this.isHttpsFirstModeEnabled_ = httpsFirstModePref.value !== undefined &&
        httpsFirstModePref.value !== HttpsFirstModeSetting.DISABLED;

    const safeBrowsingPref = this.getPref('generated.safe_browsing');
    this.isSafeBrowsingEnabled_ = safeBrowsingPref.value !== undefined &&
        safeBrowsingPref.value !== SafeBrowsingSetting.DISABLED;
    this.isSafeBrowsingWarningIconVisible_ = !this.isSafeBrowsingEnabled_ &&
        safeBrowsingPref.enforcement !==
            chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  private computeShouldHideBundles_(): boolean {
    if (this.getPref('generated.safe_browsing').enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return true;
    }

    if (this.enableBundledSecuritySettingsSecureDnsV2_ &&
        this.getPref('dns_over_https.mode').enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED) {
      return true;
    }

    if (this.getPref('generated.javascript_optimizer').enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED &&
        this.getPref('generated.javascript_optimizer').controlledBy !==
            chrome.settingsPrivate.ControlledBy.SAFE_BROWSING_OFF) {
      return true;
    }

    return false;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);

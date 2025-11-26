// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../controls/collapse_radio_button.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import '../../simple_confirmation_dialog.js';
import './secure_dns.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import type {PrivacyPageBrowserProxy} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCollapseRadioButtonElement} from '../../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, SafeBrowsingInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixin, Router} from '../../router.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import {ContentSettingsTypes} from '../../site_settings/constants.js';
import type {SiteSettingsBrowserProxy} from '../../site_settings/site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from '../../site_settings/site_settings_browser_proxy.js';
import {isSettingEnabled} from '../../site_settings/site_settings_util.js';
import {SafeBrowsingSetting} from '../safe_browsing_types.js';

import {getTemplate} from './security_page.html.js';

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

const SettingsSecurityPageElementBase =
    HelpBubbleMixin(RouteObserverMixin(SettingsViewMixin(
        WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement))))));

export class SettingsSecurityPageElement extends
    SettingsSecurityPageElementBase {
  static get is() {
    return 'settings-security-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the secure DNS setting should be displayed.
       */
      showSecureDnsSetting_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showSecureDnsSetting');
        },
      },

      // <if expr="is_chromeos">
      /**
       * Whether a link to secure DNS OS setting should be displayed.
       */
      showSecureDnsSettingLink_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showSecureDnsSettingLink');
        },
      },
      // </if>

      /**
       * Valid safe browsing states.
       */
      safeBrowsingSettingEnum_: {
        type: Object,
        value: SafeBrowsingSetting,
      },

      /**
       * Valid HTTPS-First Mode states.
       */
      httpsFirstModeSettingEnum_: {
        type: Object,
        value: HttpsFirstModeSetting,
      },

      /**
       * Setting for HTTPS-First Mode when the toggle is off.
       */
      httpsFirstModeUncheckedValues_: {
        type: Array,
        value: () => [HttpsFirstModeSetting.DISABLED],
      },

      javascriptOptimizerSubLabel_: {
        type: String,
        value: '',
      },

      enableHttpsFirstModeNewSettings_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableHttpsFirstModeNewSettings');
        },
      },

      enableSecurityKeysSubpage_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysSubpage');
        },
      },

      enableHashPrefixRealTimeLookups_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableHashPrefixRealTimeLookups');
        },
      },

      hideExtendedReportingRadioButton_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
                     'extendedReportingRemovePrefDependency') &&
              loadTimeData.getBoolean('hashPrefixRealTimeLookupsSamplePing');
        },
      },

      showDisableSafebrowsingDialog_: Boolean,
    };
  }
  declare private showSecureDnsSetting_: boolean;

  // <if expr="is_chromeos">
  declare private showSecureDnsSettingLink_: boolean;
  // </if>

  declare private enableSecurityKeysSubpage_: boolean;
  declare private showDisableSafebrowsingDialog_: boolean;
  declare private enableHashPrefixRealTimeLookups_: boolean;
  declare private httpsFirstModeUncheckedValues_: HttpsFirstModeSetting[];
  declare private enableHttpsFirstModeNewSettings_: boolean;
  declare private javascriptOptimizerSubLabel_: string;
  declare private hideExtendedReportingRadioButton_: boolean;

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteBrowserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    CrSettingsPrefs.initialized.then(() => {
      // Expand initial pref value manually because automatic
      // expanding is disabled.
      const prefValue = this.getPref('generated.safe_browsing').value;
      if (prefValue === SafeBrowsingSetting.ENHANCED) {
        this.$.safeBrowsingEnhanced.expanded = true;
      } else if (prefValue === SafeBrowsingSetting.STANDARD) {
        this.$.safeBrowsingStandard.expanded = true;
      }

      // The HTTPS-First Mode generated pref should never be set to
      // ENABLED_BALANCED if the feature flag is not enabled.
      if (!loadTimeData.getBoolean('enableHttpsFirstModeNewSettings')) {
        assert(
            this.getPref('generated.https_first_mode_enabled').value !==
            HttpsFirstModeSetting.ENABLED_BALANCED);
      }
    });

    this.registerHelpBubble(
        'kEnhancedProtectionSettingElementId',
        this.$.safeBrowsingEnhanced.getBubbleAnchor(), {anchorPaddingTop: 10});

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
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED);
    const queryParams = Router.getInstance().getQueryParameters();
    const section = queryParams.get('q');
    if (section === 'enhanced') {
      this.$.safeBrowsingEnhanced.expanded = false;
      this.$.safeBrowsingStandard.expanded = false;
    }
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
  private onSafeBrowsingRadioChange_() {
    const selected =
        Number.parseInt(this.$.safeBrowsingRadioGroup.selected || '', 10);
    const prefValue = this.getPref('generated.safe_browsing').value;
    if (prefValue !== selected) {
      this.recordInteractionHistogramOnRadioChange_(selected);
      this.recordActionOnRadioChange_(selected);
    }
    if (selected === SafeBrowsingSetting.DISABLED) {
      this.showDisableSafebrowsingDialog_ = true;
    } else {
      this.updateCollapsedButtons_();
      this.$.safeBrowsingRadioGroup.sendPrefChange();
    }
  }

  private getDisabledExtendedSafeBrowsing_(): boolean {
    return this.getPref('generated.safe_browsing').value !==
        SafeBrowsingSetting.STANDARD;
  }

  private getSafeBrowsingStandardSubLabel_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'safeBrowsingStandardDescProxy' :
            'safeBrowsingStandardDesc');
  }

  private getPasswordsLeakToggleSubLabel_(): string {
    let subLabel = this.i18n('passwordsLeakDetectionGeneralDescription');
    // If the backing password leak detection preference is enabled, but the
    // generated preference is off and user control is disabled, then additional
    // text explaining that the feature will be enabled if the user signs in is
    // added.
    if (this.prefs !== undefined) {
      const generatedPref = this.getPref('generated.password_leak_detection');
      if (this.getPref('profile.password_manager_leak_detection').value &&
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
  private getName_(value: number): string {
    return value.toString();
  }

  private getHttpsFirstModeSubLabel_(): string {
    // If the backing HTTPS-Only Mode preference is enabled, but the
    // generated preference has its user control disabled, then additional
    // text explaining that the feature is locked down for Advanced Protection
    // users is added.
    const generatedPref = this.getPref('generated.https_first_mode_enabled');
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

  private isHttpsFirstModeExpanded_(value: number): boolean {
    // If the pref is not user-modifiable, we should only show the main toggle.
    // (Note: this is not the case when the setting is policy-managed -- the
    // radio group should be expanded and labeled with the enterprise
    // indicator.)
    const generatedPref = this.getPref('generated.https_first_mode_enabled');
    if (generatedPref.userControlDisabled) {
      return false;
    }
    return value !== HttpsFirstModeSetting.DISABLED;
  }

  private onManageCertificatesClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('certManagementV2URL'));
  }

  private onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  }

  private onJavascriptOptimizerSettingsClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER);
  }

  private onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  }

  private onEnhancedProtectionLearnMoreClick_(e: Event) {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('enhancedProtectionHelpCenterURL'));
    e.preventDefault();
  }

  private onSafeBrowsingExtendedReportingChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IMPROVE_SECURITY);
  }

  /**
   * Handles the closure of the disable safebrowsing dialog, reselects the
   * appropriate radio button if the user cancels the dialog, and puts focus on
   * the disable safebrowsing button.
   */
  private onDisableSafebrowsingDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
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

  private onEnhancedProtectionExpandButtonClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.ENHANCED);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.ENHANCED);
  }

  private onStandardProtectionExpandButtonClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.STANDARD);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.STANDARD);
  }

  // <if expr="is_chromeos">
  private onOpenChromeOsSecureDnsSettingsClicked_() {
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
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page': SettingsSecurityPageElement;
  }
}

customElements.define(
    SettingsSecurityPageElement.is, SettingsSecurityPageElement);

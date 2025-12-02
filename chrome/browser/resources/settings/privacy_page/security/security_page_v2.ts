// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../icons.html.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_subpage.js';
import './security_page_feature_row.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixin} from '../../router.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import type {HatsBrowserProxy} from '../hats_browser_proxy.js';
import {HatsBrowserProxyImpl, SecurityPageV2Interaction} from '../hats_browser_proxy.js';
import {SafeBrowsingSetting} from '../safe_browsing_types.js';

import type {SecurityPageFeatureRowElement} from './security_page_feature_row.js';
import {getTemplate} from './security_page_v2.html.js';

/** Enumeration of all security settings bundle modes.*/
// LINT.IfChange(SecuritySettingsBundleSetting)
export enum SecuritySettingsBundleSetting {
  STANDARD = 0,
  ENHANCED = 1,
}
// LINT.ThenChange(/components/safe_browsing/core/common/safe_browsing_prefs.h:SecuritySettingsBundleSetting)

export interface SettingsSecurityPageV2Element {
  $: {
    bundlesRadioGroup: SettingsRadioGroupElement,
    passwordsLeakToggle: SettingsToggleButtonElement,
    resetEnhancedBundleToDefaultsButton: CrButtonElement,
    resetStandardBundleToDefaultsButton: CrButtonElement,
    securitySettingsBundleEnhanced: ControlledRadioButtonElement,
    securitySettingsBundleStandard: ControlledRadioButtonElement,
    safeBrowsingRadioGroup: SettingsRadioGroupElement,
    safeBrowsingRow: SecurityPageFeatureRowElement,
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

      safeBrowsingOff_: {
        type: Array,
        value: () => [SafeBrowsingSetting.DISABLED],
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
    };
  }

  static get observers() {
    return [
      'updateResetButtonVisibility_(' +
          'isResettingToDefaults_,' +
          'prefs.generated.security_settings_bundle.value,' +
          'prefs.generated.safe_browsing.*),',
    ];
  }

  declare private isResettingToDefaults_: boolean;
  declare private isResetStandardBundleToDefaultsButtonVisible_: boolean;
  declare private isResetEnhancedBundleToDefaultsButtonVisible_: boolean;
  declare private safeBrowsingOff_: SafeBrowsingSetting[];
  declare private safeBrowsingStateTextMap_: Object;

  private lastFocusTime_: number|undefined;
  private totalTimeInFocus_: number = 0;
  private interactions_: Set<SecurityPageV2Interaction> = new Set();
  private safeBrowsingStateOnOpen_: SafeBrowsingSetting;
  private securitySettingsBundleStateOnOpen_: SecuritySettingsBundleSetting;
  private isRouteSecurity_: boolean = true;
  private eventTracker_: EventTracker = new EventTracker();
  private hatsBrowserProxy_: HatsBrowserProxy =
      HatsBrowserProxyImpl.getInstance();

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
    } else if (selected === SafeBrowsingSetting.ENHANCED) {
      this.interactions_.add(
          SecurityPageV2Interaction.ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK);
    }
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
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

  private updateResetButtonVisibility_() {
    this.isResetStandardBundleToDefaultsButtonVisible_ = false;
    this.isResetEnhancedBundleToDefaultsButtonVisible_ = false;

    if (this.isResettingToDefaults_) {
      return;
    }

    const bundleSetting = this.getBundleSetting_();

    const prefsToCheck = [{
      prefKey: 'generated.safe_browsing',
      defaultValue: this.getDefaultSafeBrowsingValue_(bundleSetting),
    }];
    for (const prefToCheck of prefsToCheck) {
      const pref = this.getPref(prefToCheck.prefKey);
      if (pref.value !== prefToCheck.defaultValue &&
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
    this.isResettingToDefaults_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);

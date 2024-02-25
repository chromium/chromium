// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import './browser_tabs_model_form.js';
import './camera_roll_manager_form.js';
import './i18n_setup.js';
import './phone_name_form.js';
import './phone_status_model_form.js';
import './notification_manager.js';
import './shared_style.css.js';
import './quick_action_controller_form.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {getTemplate} from './phonehub_tab.html.js';
import {FeatureStatus} from './types.js';

/**
 * Maps a FeatureStatus to it's title label in the dropdown.
 * @type {!Map<FeatureStatus, String>}
 */
const featureStatusToStringMap = new Map([
  [FeatureStatus.NOT_ELIGIBLE_FOR_FEATURE, 'Not eligible for feature'],
  [
    FeatureStatus.ELIGIBLE_PHONE_BUT_NOT_SETUP,
    'Eligible for phone but not setup',
  ],
  [
    FeatureStatus.PHONE_SELECTED_AND_PENDING_SETUP,
    'Phone selected and pending setup',
  ],
  [FeatureStatus.DISABLED, 'Disabled'],
  [FeatureStatus.UNAVAILABLE_BLUETOOTH_OFF, 'Unavailable bluetooth off'],
  [FeatureStatus.ENABLED_BUT_DISCONNECTED, 'Enabled but disconnected'],
  [FeatureStatus.ENABLED_AND_CONNECTING, 'Enabled and connecting'],
  [FeatureStatus.ENABLED_AND_CONNECTED, 'Enabled and connected'],
]);

Polymer({
  is: 'phonehub-tab',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    isPhoneHubEnabled_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isPhoneHubEnabled'),
      readonly: true,
    },

    /** @private */
    shouldEnableFakePhoneHubManager_: {
      type: Boolean,
      value: false,
      observer: 'onShouldEnableFakePhoneHubManagerChanged_',
    },

    /** @private */
    shouldShowOnboardingFlow_: {
      type: Boolean,
      value: false,
      observer: 'onShouldShowOnboardingFlowChanged_',
    },

    /**
     * Must stay in order with FeatureStatus.
     * @private
     */
    featureStatusList_: {
      type: Array,
      value: () => {
        return [
          FeatureStatus.NOT_ELIGIBLE_FOR_FEATURE,
          FeatureStatus.ELIGIBLE_PHONE_BUT_NOT_SETUP,
          FeatureStatus.PHONE_SELECTED_AND_PENDING_SETUP,
          FeatureStatus.DISABLED,
          FeatureStatus.UNAVAILABLE_BLUETOOTH_OFF,
          FeatureStatus.ENABLED_BUT_DISCONNECTED,
          FeatureStatus.ENABLED_AND_CONNECTING,
          FeatureStatus.ENABLED_AND_CONNECTED,
        ];
      },
      readonly: true,
    },

    /** @private {!FeatureStatus} */
    featureStatus_: {
      type: Number,
      value: FeatureStatus.ENABLED_AND_CONNECTED,
    },

    /** @private */
    isPhoneSetUp_: {
      type: Boolean,
      computed: 'isPhoneSetUpComputed_(featureStatus_)',
    },

    /** @private */
    canOnboardingFlowBeShown_: {
      type: Boolean,
      computed: 'canOnboardingFlowBeShownComputed_(featureStatus_)',
    },

    /** @private */
    isFeatureEnabledAndConnected_: {
      type: Boolean,
      computed: 'isFeatureEnabledAndConnectedComputed_(featureStatus_)',
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
      'should-show-onboarding-ui-changed',
      this.onShouldShowOnboardingUiChanged_.bind(this));
  },

  /**
   * @return {boolean}
   * @private
   */
  canOnboardingFlowBeShownComputed_() {
    if (this.featureStatus_ === FeatureStatus.DISABLED ||
      this.featureStatus_ === FeatureStatus.ELIGIBLE_PHONE_BUT_NOT_SETUP) {
      return true;
    }
    return false;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPhoneSetUpComputed_() {
    if (this.featureStatus_ === FeatureStatus.NOT_ELIGIBLE_FOR_FEATURE ||
      this.featureStatus_ === FeatureStatus.ELIGIBLE_PHONE_BUT_NOT_SETUP) {
      return false;
    }

    return true;
  },

  /**
   * @return {boolean}
   * @private
   */
  isFeatureEnabledAndConnectedComputed_() {
    return this.featureStatus_ === FeatureStatus.ENABLED_AND_CONNECTED;
  },

  /** @private */
  onShouldEnableFakePhoneHubManagerChanged_() {
    this.browserProxy_.setFakePhoneHubManagerEnabled(
      this.shouldEnableFakePhoneHubManager_);

    if (!this.shouldEnableFakePhoneHubManager_) {
      return;
    }

    // Propgagate default values to fake PhoneHub manager.
    flush();
    this.onFeatureStatusSelected_();
    this.onShouldShowOnboardingFlowChanged_();
  },

  /** @private */
  onFeatureStatusSelected_() {
    const select = /** @type {!HTMLSelectElement} */
      (this.$$('#featureStatusList'));
    this.featureStatus_ = this.featureStatusList_[select.selectedIndex];
    this.browserProxy_.setFeatureStatus(this.featureStatus_);
  },

  /**
   * @param {FeatureStatus} featureStatus The feature status enum.
   * @private
   */
  getFeatureStatusName_(featureStatus) {
    return featureStatusToStringMap.get(featureStatus);
  },

  /** @private */
  onPhoneHubFlagButtonClick_() {
    window.open('chrome://flags/#enable-phone-hub');
  },

  /**
   * @param {boolean} shouldShowOnboardingUi
   * @private
   */
  onShouldShowOnboardingUiChanged_(shouldShowOnboardingUi) {
    if (this.shouldShowOnboardingFlow_ !== shouldShowOnboardingUi) {
      this.shouldShowOnboardingFlow_ = shouldShowOnboardingUi;
    }
  },

  /** @private */
  onResetHasMultideviceFeatureSetupUiBeenDismissedButtonClick_() {
    this.browserProxy_.resetHasMultideviceFeatureSetupUiBeenDismissed();
  },

  /** @private */
  onResetShouldShowOnboardingUiButtonClick_() {
    this.browserProxy_.resetShouldShowOnboardingUi();
  },

  /** @private */
  onResetCameraRollOnboardingUiDismissedButtonClick_() {
    this.browserProxy_.resetCameraRollOnboardingUiDismissed();
  },

  /** @private */
  onShouldShowOnboardingFlowChanged_() {
    if (!this.shouldEnableFakePhoneHubManager_) {
      return;
    }
    this.browserProxy_.setShowOnboardingFlow(this.shouldShowOnboardingFlow_);
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  },
});

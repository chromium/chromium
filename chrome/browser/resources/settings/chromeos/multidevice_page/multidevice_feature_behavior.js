// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for dealing with MultiDevice features. It is
 * intended to facilitate passing data between elements in the MultiDevice page
 * cleanly and concisely. It includes some constants and utility methods.
 */

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';

import {MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus} from './multidevice_constants.js';

/** @polymerBehavior */
const MultiDeviceFeatureBehaviorImpl = {
  properties: {
    /** @type {!MultiDevicePageContentData} */
    pageContentData: Object,

    /**
     * Enum defined in multidevice_constants.js.
     * @type {Object<string, number>}
     */
    MultiDeviceFeature: {
      type: Object,
      value: MultiDeviceFeature,
    },
  },

  /**
   * Whether the gatekeeper pref for the whole Better Together feature suite is
   * on.
   * @return {boolean}
   */
  isSuiteOn() {
    return !!this.pageContentData &&
        this.pageContentData.betterTogetherState ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Whether the gatekeeper pref for the whole Better Together feature suite is
   * allowed by policy.
   * @return {boolean}
   */
  isSuiteAllowedByPolicy() {
    return !!this.pageContentData &&
        this.pageContentData.betterTogetherState !==
        MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * Whether an individual feature is allowed by policy.
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureAllowedByPolicy(feature) {
    return this.getFeatureState(feature) !==
        MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureSupported(feature) {
    return ![MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
             MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE,
    ].includes(this.getFeatureState(feature));
  },

  /**
   * Whether the top-level Phone Hub feature is enabled.
   * @return {boolean}
   */
  isPhoneHubOn() {
    return this.getFeatureState(MultiDeviceFeature.PHONE_HUB) ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isPhoneHubSubFeature(feature) {
    return [
      MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL,
      MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
      MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION,
      MultiDeviceFeature.ECHE,
    ].includes(feature);
  },

  /**
   * @return {boolean} Whether or not Phone Hub notification access is
   *     prohibited (i.e., due to the user having a work profile).
   */
  isPhoneHubNotificationAccessProhibited() {
    return this.pageContentData &&
        this.pageContentData.notificationAccessStatus ===
        PhoneHubFeatureAccessStatus.PROHIBITED;
  },

  /**
   * @return {boolean} Whether or not Phone Hub apps access is
   *     prohibited (i.e., due to the apps streaming policy of the phone is
   * disabled).
   */
  isPhoneHubAppsAccessProhibited() {
    return this.pageContentData &&
        this.pageContentData.appsAccessStatus ===
        PhoneHubFeatureAccessStatus.PROHIBITED;
  },

  /**
   * Whether Camera Roll requires user action to finish set up.
   * @return {boolean}
   */
  isPhoneHubCameraRollSetupRequired() {
    return this.isFeatureSupported(MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL) &&
        this.pageContentData.cameraRollAccessStatus ===
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED &&
        this.isFeatureAllowedByPolicy(MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL);
  },

  /**
   * Whether Apps requires user action to finish set up.
   * @return {boolean}
   */
  isPhoneHubAppsSetupRequired() {
    return this.isFeatureSupported(MultiDeviceFeature.ECHE) &&
        this.pageContentData.isPhoneHubPermissionsDialogSupported &&
        this.pageContentData.appsAccessStatus ===
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED &&
        this.isFeatureAllowedByPolicy(MultiDeviceFeature.ECHE);
  },

  /**
   * Whether Notifications requires user action to finish set up.
   * @return {boolean}
   */
  isPhoneHubNotificationsSetupRequired() {
    return this.pageContentData.notificationAccessStatus ===
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED &&
        this.isFeatureAllowedByPolicy(
            MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS);
  },

  /**
   * Whether the user is prevented from attempted to change a given feature. In
   * the UI this corresponds to a disabled toggle.
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureStateEditable(feature) {
    // The suite is off and the toggle corresponds to an individual feature
    // (as opposed to the full suite).
    if (feature !== MultiDeviceFeature.BETTER_TOGETHER_SUITE &&
        !this.isSuiteOn()) {
      return false;
    }

    // Cannot edit Phone Hub sub-feature toggles if the top-level Phone Hub
    // feature is not enabled.
    if (this.isPhoneHubSubFeature(feature) && !this.isPhoneHubOn()) {
      return false;
    }

    // Cannot edit the Phone Hub notification toggle if notification access is
    // prohibited.
    if (feature === MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS &&
        this.isPhoneHubNotificationAccessProhibited()) {
      return false;
    }

    // Cannot edit the Phone Hub apps toggle if apps access is
    // prohibited.
    if (feature === MultiDeviceFeature.ECHE &&
        this.isPhoneHubAppsAccessProhibited()) {
      return false;
    }

    return [
      MultiDeviceFeatureState.DISABLED_BY_USER,
      MultiDeviceFeatureState.ENABLED_BY_USER,
    ].includes(this.getFeatureState(feature));
  },

  /**
   * The localized string representing the name of the feature.
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureName(feature) {
    switch (feature) {
      case MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return this.i18n('multideviceSetupItemHeading');
      case MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18n('multideviceInstantTetheringItemTitle');
      case MultiDeviceFeature.MESSAGES:
        return this.i18n('multideviceAndroidMessagesItemTitle');
      case MultiDeviceFeature.SMART_LOCK:
        return this.i18n('multideviceSmartLockItemTitle');
      case MultiDeviceFeature.PHONE_HUB:
        return this.i18n('multidevicePhoneHubItemTitle');
      case MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL:
        return this.i18n('multidevicePhoneHubCameraRollItemTitle');
      case MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.i18n('multidevicePhoneHubNotificationsItemTitle');
      case MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.i18n('multidevicePhoneHubTaskContinuationItemTitle');
      case MultiDeviceFeature.WIFI_SYNC:
        return this.i18n('multideviceWifiSyncItemTitle');
      case MultiDeviceFeature.ECHE:
        return this.i18n('multidevicePhoneHubAppsItemTitle');
      default:
        return '';
    }
  },

  /**
   * The full icon name used provided by the containing iron-iconset-svg
   * (i.e. [iron-iconset-svg name]:[SVG <g> tag id]) for a given feature.
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getIconName(feature) {
    switch (feature) {
      case MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return 'os-settings:multidevice-better-together-suite';
      case MultiDeviceFeature.MESSAGES:
        return 'os-settings:multidevice-messages';
      case MultiDeviceFeature.SMART_LOCK:
        return 'os-settings:multidevice-smart-lock';
      case MultiDeviceFeature.PHONE_HUB:
      case MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL:
      case MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
      case MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
      case MultiDeviceFeature.ECHE:
        return 'os-settings:multidevice-better-together-suite';
      case MultiDeviceFeature.WIFI_SYNC:
        return 'os-settings:multidevice-wifi-sync';
      default:
        return '';
    }
  },

  /**
   * The localized string providing a description or useful status information
   * concerning a given feature.
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureSummaryHtml(feature) {
    switch (feature) {
      case MultiDeviceFeature.SMART_LOCK:
        return this.i18nAdvanced('multideviceSmartLockItemSummary');
      case MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18nAdvanced('multideviceInstantTetheringItemSummary');
      case MultiDeviceFeature.MESSAGES:
        return this.i18nAdvanced('multideviceAndroidMessagesItemSummary');
      case MultiDeviceFeature.PHONE_HUB:
        return this.i18nAdvanced('multidevicePhoneHubItemSummary');
      case MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL:
        return this.i18nAdvanced('multidevicePhoneHubCameraRollItemSummary');
      case MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.i18nAdvanced('multidevicePhoneHubNotificationsItemSummary');
      case MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.i18nAdvanced(
            'multidevicePhoneHubTaskContinuationItemSummary');
      case MultiDeviceFeature.WIFI_SYNC:
        return this.i18nAdvanced('multideviceWifiSyncItemSummary');
      case MultiDeviceFeature.ECHE:
        return this.i18nAdvanced('multidevicePhoneHubAppsItemSummary');
      default:
        return '';
    }
  },

  /**
   * Extracts the MultiDeviceFeatureState enum value describing the given
   * feature from this.pageContentData. Returns null if the feature is not
   * an accepted value (e.g. testing fake).
   * @param {!MultiDeviceFeature} feature
   * @return {?MultiDeviceFeatureState}
   */
  getFeatureState(feature) {
    if (!this.pageContentData) {
      return null;
    }

    switch (feature) {
      case MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return this.pageContentData.betterTogetherState;
      case MultiDeviceFeature.INSTANT_TETHERING:
        return this.pageContentData.instantTetheringState;
      case MultiDeviceFeature.MESSAGES:
        return this.pageContentData.messagesState;
      case MultiDeviceFeature.SMART_LOCK:
        return this.pageContentData.smartLockState;
      case MultiDeviceFeature.PHONE_HUB:
        return this.pageContentData.phoneHubState;
      case MultiDeviceFeature.PHONE_HUB_CAMERA_ROLL:
        return this.pageContentData.phoneHubCameraRollState;
      case MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.pageContentData.phoneHubNotificationsState;
      case MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.pageContentData.phoneHubTaskContinuationState;
      case MultiDeviceFeature.WIFI_SYNC:
        return this.pageContentData.wifiSyncState;
      case MultiDeviceFeature.ECHE:
        return this.pageContentData.phoneHubAppsState;
      default:
        return null;
    }
  },

  /**
   * Whether a host phone has been set by the user (not necessarily verified).
   * @return {boolean}
   */
  isHostSet() {
    return [
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ].includes(this.pageContentData.mode);
  },
};

/** @interface */
export class MultiDeviceFeatureBehaviorInterface {
  constructor() {
    /** @type {!MultiDevicePageContentData} */
    this.pageContentData;

    /** @type {Object<string, number>} */
    this.MultiDeviceFeature;
  }

  /**
   * @return {boolean}
   */
  isSuiteOn() {}

  /**
   * @return {boolean}
   */
  isSuiteAllowedByPolicy() {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureAllowedByPolicy(feature) {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureSupported(feature) {}

  /**
   * @return {boolean}
   */
  isPhoneHubOn() {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isPhoneHubSubFeature(feature) {}

  /**
   * @return {boolean}
   */
  isPhoneHubNotificationAccessProhibited() {}

  /**
   * @return {boolean}
   */
  isPhoneHubAppsAccessProhibited() {}

  /**
   * @return {boolean}
   */
  isPhoneHubCameraRollSetupRequired() {}

  /**
   * @return {boolean}
   */
  isPhoneHubAppsSetupRequired() {}

  /**
   * @return {boolean}
   */
  isPhoneHubNotificationsSetupRequired() {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureStateEditable(feature) {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureName(feature) {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getIconName(feature) {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureSummaryHtml(feature) {}

  /**
   * @param {!MultiDeviceFeature} feature
   * @return {?MultiDeviceFeatureState}
   */
  getFeatureState(feature) {}

  /**
   * @return {boolean}
   */
  isHostSet() {}
}

/** @polymerBehavior */
export const MultiDeviceFeatureBehavior = [
  I18nBehavior,
  MultiDeviceFeatureBehaviorImpl,
];

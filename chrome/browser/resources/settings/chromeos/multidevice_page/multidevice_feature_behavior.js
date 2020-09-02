// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {MultiDeviceSettingsMode, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData } from './multidevice_constants.m.js';
// #import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
// clang-format on

/**
 * @fileoverview Polymer behavior for dealing with MultiDevice features. It is
 * intended to facilitate passing data between elements in the MultiDevice page
 * cleanly and concisely. It includes some constants and utility methods.
 */

/** @polymerBehavior */
const MultiDeviceFeatureBehaviorImpl = {
  properties: {
    /** @type {!settings.MultiDevicePageContentData} */
    pageContentData: Object,

    /**
     * Enum defined in multidevice_constants.js.
     * @type {Object<string, number>}
     */
    MultiDeviceFeature: {
      type: Object,
      value: settings.MultiDeviceFeature,
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
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Whether the gatekeeper pref for the whole Better Together feature suite is
   * allowed by policy.
   * @return {boolean}
   */
  isSuiteAllowedByPolicy() {
    return !!this.pageContentData &&
        this.pageContentData.betterTogetherState !==
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * Whether an individual feature is allowed by policy.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureAllowedByPolicy(feature) {
    return this.getFeatureState(feature) !==
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * @param {!settings.MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureSupported(feature) {
    return ![settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
             settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE,
    ].includes(this.getFeatureState(feature));
  },

  /**
   * Whether the user is prevented from attempted to change a given feature. In
   * the UI this corresponds to a disabled toggle.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureStateEditable(feature) {
    // The suite is off and the toggle corresponds to an individual feature
    // (as opposed to the full suite).
    if (feature !== settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE &&
        !this.isSuiteOn()) {
      return false;
    }

    return [
      settings.MultiDeviceFeatureState.DISABLED_BY_USER,
      settings.MultiDeviceFeatureState.ENABLED_BY_USER
    ].includes(this.getFeatureState(feature));
  },

  /**
   * The localized string representing the name of the feature.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureName(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return this.i18n('multideviceSetupItemHeading');
      case settings.MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18n('multideviceInstantTetheringItemTitle');
      case settings.MultiDeviceFeature.MESSAGES:
        return this.i18n('multideviceAndroidMessagesItemTitle');
      case settings.MultiDeviceFeature.SMART_LOCK:
        return this.i18n('multideviceSmartLockItemTitle');
      case settings.MultiDeviceFeature.PHONE_HUB:
        return this.i18n('multidevicePhoneHubItemTitle');
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.i18n('multidevicePhoneHubNotificationsItemTitle');
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATION_BADGE:
        return this.i18n('multidevicePhoneHubNotificationBadgeItemTitle');
      case settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.i18n('multidevicePhoneHubTaskContinuationItemTitle');
      case settings.MultiDeviceFeature.WIFI_SYNC:
        return this.i18n('multideviceWifiSyncItemTitle');
      default:
        return '';
    }
  },

  /**
   * The full icon name used provided by the containing iron-iconset-svg
   * (i.e. [iron-iconset-svg name]:[SVG <g> tag id]) for a given feature.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {string}
   */
  getIconName(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return 'os-settings:multidevice-better-together-suite';
      case settings.MultiDeviceFeature.MESSAGES:
        return 'os-settings:multidevice-messages';
      case settings.MultiDeviceFeature.SMART_LOCK:
        return 'os-settings:multidevice-smart-lock';
      // TODO(https://crbug.com/1106937): Use real Phone Hub asset.
      case settings.MultiDeviceFeature.PHONE_HUB:
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATION_BADGE:
      case settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return 'os-settings:multidevice-better-together-suite';
      // TODO(cvandermerwe): Use real Wifi Sync asset.
      case settings.MultiDeviceFeature.WIFI_SYNC:
        return 'os-settings:multidevice-wifi';
      default:
        return '';
    }
  },

  /**
   * The localized string providing a description or useful status information
   * concerning a given feature.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {string}
   */
  getFeatureSummaryHtml(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.SMART_LOCK:
        return this.i18nAdvanced('multideviceSmartLockItemSummary');
      case settings.MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18nAdvanced('multideviceInstantTetheringItemSummary');
      case settings.MultiDeviceFeature.MESSAGES:
        return this.i18nAdvanced('multideviceAndroidMessagesItemSummary');
      case settings.MultiDeviceFeature.PHONE_HUB:
        return this.i18nAdvanced('multidevicePhoneHubItemSummary');
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.i18nAdvanced('multidevicePhoneHubNotificationsItemSummary');
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATION_BADGE:
        return this.i18nAdvanced(
            'multidevicePhoneHubNotificationBadgeItemSummary');
      case settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.i18nAdvanced(
            'multidevicePhoneHubTaskContinuationItemSummary');
      case settings.MultiDeviceFeature.WIFI_SYNC:
        return this.i18nAdvanced('multideviceWifiSyncItemSummary');
      default:
        return '';
    }
  },

  /**
   * Extracts the MultiDeviceFeatureState enum value describing the given
   * feature from this.pageContentData. Returns null if the feature is not
   * an accepted value (e.g. testing fake).
   * @param {!settings.MultiDeviceFeature} feature
   * @return {?settings.MultiDeviceFeatureState}
   */
  getFeatureState(feature) {
    if (!this.pageContentData) {
      return null;
    }

    switch (feature) {
      case settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return this.pageContentData.betterTogetherState;
      case settings.MultiDeviceFeature.INSTANT_TETHERING:
        return this.pageContentData.instantTetheringState;
      case settings.MultiDeviceFeature.MESSAGES:
        return this.pageContentData.messagesState;
      case settings.MultiDeviceFeature.SMART_LOCK:
        return this.pageContentData.smartLockState;
      case settings.MultiDeviceFeature.PHONE_HUB:
        return this.pageContentData.phoneHubState;
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS:
        return this.pageContentData.phoneHubNotificationsState;
      case settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATION_BADGE:
        return this.pageContentData.phoneHubNotificationBadgeState;
      case settings.MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION:
        return this.pageContentData.phoneHubTaskContinuationState;
      case settings.MultiDeviceFeature.WIFI_SYNC:
        return this.pageContentData.wifiSyncState;
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
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ].includes(this.pageContentData.mode);
  },
};

/** @polymerBehavior */
/* #export */ const MultiDeviceFeatureBehavior = [
  I18nBehavior,
  MultiDeviceFeatureBehaviorImpl,
];

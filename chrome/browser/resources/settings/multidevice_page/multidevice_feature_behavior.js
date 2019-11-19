// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for dealing with MultiDevice features. It is
 * intended to facilitate passing data between elements in the MultiDevice page
 * cleanly and concisely. It includes some constants and utility methods.
 */
cr.exportPath('settings');

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
      value: settings.MultiDeviceFeature,
    },
  },

  /**
   * Whether the gatekeeper pref for the whole Better Together feature suite is
   * on.
   * @return {boolean}
   */
  isSuiteOn: function() {
    return !!this.pageContentData &&
        this.pageContentData.betterTogetherState ===
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * Whether the gatekeeper pref for the whole Better Together feature suite is
   * allowed by policy.
   * @return {boolean}
   */
  isSuiteAllowedByPolicy: function() {
    return !!this.pageContentData &&
        this.pageContentData.betterTogetherState !==
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * Whether an individual feature is allowed by policy.
   * @param {!settings.MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureAllowedByPolicy: function(feature) {
    return this.getFeatureState(feature) !==
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY;
  },

  /**
   * @param {!settings.MultiDeviceFeature} feature
   * @return {boolean}
   */
  isFeatureSupported: function(feature) {
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
  isFeatureStateEditable: function(feature) {
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
  getFeatureName: function(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return this.i18n('multideviceSetupItemHeading');
      case settings.MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18n('multideviceInstantTetheringItemTitle');
      case settings.MultiDeviceFeature.MESSAGES:
        return this.i18n('multideviceAndroidMessagesItemTitle');
      case settings.MultiDeviceFeature.SMART_LOCK:
        return this.i18n('multideviceSmartLockItemTitle');
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
  getIconName: function(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE:
        return 'os-settings:multidevice-better-together-suite';
      case settings.MultiDeviceFeature.MESSAGES:
        return 'os-settings:multidevice-messages';
      case settings.MultiDeviceFeature.SMART_LOCK:
        return 'os-settings:multidevice-smart-lock';
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
  getFeatureSummaryHtml: function(feature) {
    switch (feature) {
      case settings.MultiDeviceFeature.SMART_LOCK:
        return this.i18nAdvanced('multideviceSmartLockItemSummary');
      case settings.MultiDeviceFeature.INSTANT_TETHERING:
        return this.i18nAdvanced('multideviceInstantTetheringItemSummary');
      case settings.MultiDeviceFeature.MESSAGES:
        return this.i18nAdvanced('multideviceAndroidMessagesItemSummary');
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
  getFeatureState: function(feature) {
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
      default:
        return null;
    }
  },

  /**
   * Whether a host phone has been set by the user (not necessarily verified).
   * @return {boolean}
   */
  isHostSet: function() {
    return [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ].includes(this.pageContentData.mode);
  },
};

/** @polymerBehavior */
const MultiDeviceFeatureBehavior = [
  I18nBehavior,
  MultiDeviceFeatureBehaviorImpl,
];

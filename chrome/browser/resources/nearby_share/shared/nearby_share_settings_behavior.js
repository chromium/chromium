// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview NearbyShareSettingsBehavior wraps up talking to the settings
 * mojo to get values and keeps them in sync by observing for changes
 */

import {DataUsage, FastInitiationNotificationState, NearbyShareSettingsInterface, NearbyShareSettingsObserverInterface, NearbyShareSettingsObserverReceiver, NearbyShareSettingsRemote, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

import {getNearbyShareSettings, observeNearbyShareSettings} from './nearby_share_settings.js';

/**
 * @typedef {{
 *            enabled:boolean,
 *            fastInitiationNotificationState: FastInitiationNotificationState,
 *            isFastInitiationHardwareSupported:boolean,
 *            deviceName:string,
 *            dataUsage:DataUsage,
 *            visibility:Visibility,
 *            allowedContacts:Array<string>,
 *            isOnboardingComplete:boolean,
 *          }}
 */
export let NearbySettings;

/** @polymerBehavior */
export const NearbyShareSettingsBehavior = {
  properties: {
    /** @type {NearbySettings} */
    settings: {
      type: Object,
      notify: true,
      value: {},
    },
  },

  observers: ['settingsChanged_(settings.*)'],

  /** @private {?NearbyShareSettingsInterface} */
  nearbyShareSettings_: null,

  /** @private {?NearbyShareSettingsObserverReceiver} */
  observerReceiver_: null,

  attached() {
    this.nearbyShareSettings_ = getNearbyShareSettings();
    this.observerReceiver_ = observeNearbyShareSettings(
        /** @type {!NearbyShareSettingsObserverInterface} */
        (this));
    // Request the initial values and trigger onSettingsRetrieved when they
    // are all retrieved.
    Promise
        .all([
          this.nearbyShareSettings_.getEnabled(),
          this.nearbyShareSettings_.getDeviceName(),
          this.nearbyShareSettings_.getDataUsage(),
          this.nearbyShareSettings_.getVisibility(),
          this.nearbyShareSettings_.getAllowedContacts(),
          this.nearbyShareSettings_.isOnboardingComplete(),
          this.nearbyShareSettings_.getFastInitiationNotificationState(),
          this.nearbyShareSettings_.getIsFastInitiationHardwareSupported(),
        ])
        .then((results) => {
          this.set('settings.enabled', results[0].enabled);
          this.set('settings.deviceName', results[1].deviceName);
          this.set('settings.dataUsage', results[2].dataUsage);
          this.set('settings.visibility', results[3].visibility);
          this.set('settings.allowedContacts', results[4].allowedContacts);
          this.set('settings.isOnboardingComplete', results[5].completed);
          this.set(
              'settings.fastInitiationNotificationState', results[6].state);
          this.set(
              'settings.isFastInitiationHardwareSupported',
              results[7].supported);
          this.onSettingsRetrieved();
        });
  },

  detached() {
    if (this.observerReceiver_) {
      this.observerReceiver_.$.close();
    }
    if (this.nearbyShareSettings_) {
      /** @type {NearbyShareSettingsRemote} */
      (this.nearbyShareSettings_).$.close();
    }
  },

  /**
   * @param {!boolean} enabled
   */
  onEnabledChanged(enabled) {
    this.set('settings.enabled', enabled);
  },

  /**
   * @param {!boolean} supported
   */
  onIsFastInitiationHardwareSupportedChanged(supported) {
    this.set('settings.isFastInitiationHardwareSupported', supported);
  },

  /**
   * @param {!FastInitiationNotificationState} state
   */
  onFastInitiationNotificationStateChanged(state) {
    this.set('settings.fastInitiationNotificationState', state);
  },

  /**
   * @param {!string} deviceName
   */
  onDeviceNameChanged(deviceName) {
    this.set('settings.deviceName', deviceName);
  },

  /**
   * @param {!DataUsage} dataUsage
   */
  onDataUsageChanged(dataUsage) {
    this.set('settings.dataUsage', dataUsage);
  },

  /**
   * @param {!Visibility} visibility
   */
  onVisibilityChanged(visibility) {
    this.set('settings.visibility', visibility);
  },

  /**
   * @param {!Array<!string>} allowedContacts
   */
  onAllowedContactsChanged(allowedContacts) {
    this.set('settings.allowedContacts', allowedContacts);
  },

  /**
   * @param {!boolean} is_complete
   */
  onIsOnboardingCompleteChanged(is_complete) {
    this.set('settings.isOnboardingComplete', is_complete);
  },

  /**
   * TODO(vecore): Type is actually PolymerDeepPropertyChange but the externs
   *    definition needs to be fixed so the value can be cast to primitive
   *    types.
   * @param {Object} change
   * @private
   */
  settingsChanged_(change) {
    switch (change.path) {
      case 'settings.enabled':
        this.nearbyShareSettings_.setEnabled(change.value);
        break;
      case 'settings.fastInitiationNotificationState':
        this.nearbyShareSettings_.setFastInitiationNotificationState(
            change.value);
        break;
      case 'settings.deviceName':
        this.nearbyShareSettings_.setDeviceName(change.value);
        break;
      case 'settings.dataUsage':
        this.nearbyShareSettings_.setDataUsage(change.value);
        break;
      case 'settings.visibility':
        this.nearbyShareSettings_.setVisibility(change.value);
        break;
      case 'settings.allowedContacts':
        this.nearbyShareSettings_.setAllowedContacts(change.value);
        break;
      case 'settings.isOnboardingComplete':
        this.nearbyShareSettings_.setIsOnboardingComplete(change.value);
        break;
    }
  },

  /** Override in polymer element to process the initial values */
  onSettingsRetrieved() {},

};

/** @interface */
export class NearbyShareSettingsBehaviorInterface {
  constructor() {
    /** @type {!NearbySettings} */
    this.settings;
  }

  onSettingsRetrieved() {}
}

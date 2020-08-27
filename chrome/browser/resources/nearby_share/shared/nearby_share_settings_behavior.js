// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {getNearbyShareSettings, observeNearbyShareSettings} from './nearby_share_settings.m.js';
// clang-format on

/**
 * @fileoverview NearbyShareSettingsBehavior wraps up talking to the settings
 * mojo to get values and keeps them in sync by observing for changes
 */
cr.define('nearby_share', function() {
  /**
   * @typedef {{
   *            enabled:boolean,
   *            deviceName:string,
   *            dataUsage:nearbyShare.mojom.DataUsage,
   *            visibility:nearbyShare.mojom.Visibility,
   *            allowedContacts:Array<string>
   *          }}
   */
  /* #export */ let NearbySettings;

  /** @polymerBehavior */
  /* #export */ const NearbyShareSettingsBehavior = {
    properties: {
      /** @type {nearby_share.NearbySettings} */
      settings: {
        type: Object,
        notify: true,
        value: {},
      },
    },

    observers: ['settingsChanged_(settings.*)'],

    /** @private {?nearbyShare.mojom.NearbyShareSettingsInterface} */
    nearbyShareSettings_: null,

    /** @private {?nearbyShare.mojom.NearbyShareSettingsObserverReceiver} */
    observerReceiver_: null,

    attached() {
      this.nearbyShareSettings_ = nearby_share.getNearbyShareSettings();
      this.observerReceiver_ = nearby_share.observeNearbyShareSettings(
          /** @type {!nearbyShare.mojom.NearbyShareSettingsObserverInterface} */
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
          ])
          .then((results) => {
            this.set('settings.enabled', results[0].enabled);
            this.set('settings.deviceName', results[1].deviceName);
            this.set('settings.dataUsage', results[2].dataUsage);
            this.set('settings.visibility', results[3].visibility);
            this.set('settings.allowedContacts', results[4].allowedContacts);
            this.onSettingsRetrieved();
          });
    },

    detached() {
      if (this.observerReceiver_) {
        this.observerReceiver_.$.close();
      }
      if (this.nearbyShareSettings_) {
        /** @type {nearbyShare.mojom.NearbyShareSettingsRemote} */
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
     * @param {!string} deviceName
     */
    onDeviceNameChanged(deviceName) {
      this.set('settings.deviceName', deviceName);
    },

    /**
     * @param {!nearbyShare.mojom.DataUsage} dataUsage
     */
    onDataUsageChanged(dataUsage) {
      this.set('settings.dataUsage', dataUsage);
    },

    /**
     * @param {!nearbyShare.mojom.Visibility} visibility
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
      }
    },

    /** Override in polymer element to process the initial values */
    onSettingsRetrieved() {},

  };
  // #cr_define_end
  return {
    NearbyShareSettingsBehavior,
    NearbySettings,
  };
});

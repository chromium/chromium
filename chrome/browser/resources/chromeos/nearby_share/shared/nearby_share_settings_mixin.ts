// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview NearbyShareSettingsMixin wraps up talking to the settings
 * mojo to get values and keeps them in sync by observing for changes
 */

import type {DataUsage, FastInitiationNotificationState, NearbyShareSettingsInterface, NearbyShareSettingsObserverReceiver, NearbyShareSettingsRemote, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNearbyShareSettings, observeNearbyShareSettings} from './nearby_share_settings.js';

export interface NearbySettings {
  enabled: boolean;
  fastInitiationNotificationState: FastInitiationNotificationState;
  isFastInitiationHardwareSupported: boolean;
  deviceName: string;
  dataUsage: DataUsage;
  visibility: Visibility;
  allowedContacts: string[];
  isOnboardingComplete: boolean;
}

export interface NearbyShareSettingsMixinInterface {
  settings: NearbySettings;
  onSettingsRetrieved(): void;
}

type Constructor<T> = new (...args: any[]) => T;

export const NearbyShareSettingsMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<NearbyShareSettingsMixinInterface> => {
      class NearbyShareSettingsMixinInternal extends superClass implements
          NearbyShareSettingsMixinInterface {
        static get properties() {
          return {
            settings: {
              type: Object,
              notify: true,
              value: {},
            },
          };
        }

        static get observers() {
          return ['settingsChanged_(settings.*)'];
        }

        settings: NearbySettings;
        private nearbyShareSettings_: NearbyShareSettingsInterface|null;
        private observerReceiver_: NearbyShareSettingsObserverReceiver|null;

        constructor(...args: any[]) {
          super(...args);

          this.nearbyShareSettings_ = null;
          this.observerReceiver_ = null;
        }

        override connectedCallback(): void {
          super.connectedCallback();

          this.nearbyShareSettings_ = getNearbyShareSettings();
          this.observerReceiver_ = observeNearbyShareSettings(this);

          // Request the initial values and trigger onSettingsRetrieved when
          // they are all retrieved.
          Promise
              .all([
                this.nearbyShareSettings_.getEnabled(),
                this.nearbyShareSettings_.getDeviceName(),
                this.nearbyShareSettings_.getDataUsage(),
                this.nearbyShareSettings_.getVisibility(),
                this.nearbyShareSettings_.getAllowedContacts(),
                this.nearbyShareSettings_.isOnboardingComplete(),
                this.nearbyShareSettings_.getFastInitiationNotificationState(),
                this.nearbyShareSettings_
                    .getIsFastInitiationHardwareSupported(),
              ])
              .then((results) => {
                this.set('settings.enabled', results[0].enabled);
                this.set('settings.deviceName', results[1].deviceName);
                this.set('settings.dataUsage', results[2].dataUsage);
                this.set('settings.visibility', results[3].visibility);
                this.set(
                    'settings.allowedContacts', results[4].allowedContacts);
                this.set('settings.isOnboardingComplete', results[5].completed);
                this.set(
                    'settings.fastInitiationNotificationState',
                    results[6].state);
                this.set(
                    'settings.isFastInitiationHardwareSupported',
                    results[7].supported);
                this.onSettingsRetrieved();
              });
        }

        override disconnectedCallback(): void {
          super.disconnectedCallback();

          if (this.observerReceiver_) {
            this.observerReceiver_.$.close();
          }
          if (this.nearbyShareSettings_) {
            (this.nearbyShareSettings_ as NearbyShareSettingsRemote).$.close();
          }
        }

        onEnabledChanged(enabled: boolean): void {
          this.set('settings.enabled', enabled);
        }

        onIsFastInitiationHardwareSupportedChanged(supported: boolean): void {
          this.set('settings.isFastInitiationHardwareSupported', supported);
        }

        onFastInitiationNotificationStateChanged(
            state: FastInitiationNotificationState): void {
          this.set('settings.fastInitiationNotificationState', state);
        }

        onDeviceNameChanged(deviceName: string): void {
          this.set('settings.deviceName', deviceName);
        }

        onDataUsageChanged(dataUsage: DataUsage): void {
          this.set('settings.dataUsage', dataUsage);
        }

        onVisibilityChanged(visibility: Visibility): void {
          this.set('settings.visibility', visibility);
        }

        onAllowedContactsChanged(allowedContacts: string[]): void {
          this.set('settings.allowedContacts', allowedContacts);
        }

        onIsOnboardingCompleteChanged(isComplete: boolean): void {
          this.set('settings.isOnboardingComplete', isComplete);
        }

        /**
         * TODO(vecore): Type is actually PolymerDeepPropertyChange but the
         * externs definition needs to be fixed so the value can be cast to
         * primitive types.
         */
        private settingsChanged_(change: {path: string, value: unknown}): void {
          switch (change.path) {
            case 'settings.enabled':
              this.nearbyShareSettings_!.setEnabled(change.value as boolean);
              break;
            case 'settings.fastInitiationNotificationState':
              this.nearbyShareSettings_!.setFastInitiationNotificationState(
                  change.value as number);
              break;
            case 'settings.deviceName':
              this.nearbyShareSettings_!.setDeviceName(change.value as string);
              break;
            case 'settings.dataUsage':
              this.nearbyShareSettings_!.setDataUsage(change.value as number);
              break;
            case 'settings.visibility':
              this.nearbyShareSettings_!.setVisibility(change.value as number);
              break;
            case 'settings.allowedContacts':
              this.nearbyShareSettings_!.setAllowedContacts(
                  change.value as string[]);
              break;
            case 'settings.isOnboardingComplete':
              this.nearbyShareSettings_!.setIsOnboardingComplete(
                  change.value as boolean);
              break;
          }
        }

        /** Override in polymer element to process the initial values */
        onSettingsRetrieved(): void {}
      }

      return NearbyShareSettingsMixinInternal;
    });

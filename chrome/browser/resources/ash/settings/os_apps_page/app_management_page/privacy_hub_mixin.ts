// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains utilities related to privacy hub sensor access
 * controls.
 */

import {assertNotReached} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from '../../os_privacy_page/privacy_hub_browser_proxy.js';
import {GeolocationAccessLevel} from '../../os_privacy_page/privacy_hub_geolocation_subpage.js';

type Constructor<T> = new (...args: any[]) => T;

export interface PrivacyHubMixinInterface {
  cameraSwitchForceDisabled: boolean;
  microphoneHardwareToggleActive: boolean;
  microphoneMutedBySecurityCurtain: boolean;
  isSensorBlocked(permissionType: PermissionTypeIndex|undefined): boolean;
}

export const PrivacyHubMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacyHubMixinInterface> => {
      const superClassBase = WebUiListenerMixin(PrefsMixin(superClass));

      class PrivacyHubMixin extends superClassBase implements
          PrivacyHubMixinInterface {
        static get properties() {
          return {
            cameraSwitchForceDisabled: {
              type: Boolean,
              value: false,
            },

            microphoneHardwareToggleActive: {
              type: Boolean,
              value: false,
            },

            microphoneMutedBySecurityCurtain: {
              type: Boolean,
              value: false,
            },
          };
        }

        cameraSwitchForceDisabled: boolean;
        microphoneHardwareToggleActive: boolean;
        microphoneMutedBySecurityCurtain: boolean;
        private privacyHubBrowserProxy_: PrivacyHubBrowserProxy;

        constructor() {
          super();
          this.privacyHubBrowserProxy_ =
              PrivacyHubBrowserProxyImpl.getInstance();
        }

        override connectedCallback(): void {
          super.connectedCallback();

          this.addWebUiListener(
              'force-disable-camera-switch', (disabled: boolean) => {
                this.cameraSwitchForceDisabled = disabled;
              });
          this.privacyHubBrowserProxy_
              .getInitialCameraSwitchForceDisabledState()
              .then((disabled) => {
                this.cameraSwitchForceDisabled = disabled;
              });

          this.addWebUiListener(
              'microphone-hardware-toggle-changed', (enabled: boolean) => {
                this.microphoneHardwareToggleActive = enabled;
              });
          this.privacyHubBrowserProxy_.getInitialMicrophoneHardwareToggleState()
              .then((enabled: boolean) => {
                this.microphoneHardwareToggleActive = enabled;
              });
          this.addWebUiListener(
              'microphone-muted-by-security-curtain-changed',
              (muted: boolean) => {
                this.microphoneMutedBySecurityCurtain = muted;
              });
          this.privacyHubBrowserProxy_
              .getInitialMicrophoneMutedBySecurityCurtainState()
              .then((muted: boolean) => {
                this.microphoneHardwareToggleActive = muted;
              });
        }

        // Access to Camera, Microphone and Location can be blocked system wide
        // from privacy hub. This function returns true if the relevant sensor
        // is blocked.
        isSensorBlocked(permissionType: PermissionTypeIndex|
                        undefined): boolean {
          if (permissionType === undefined || !this.prefs) {
            return false;
          }

          switch (PermissionType[permissionType]) {
            case PermissionType.kCamera:
              return !this.getPref('ash.user.camera_allowed').value;
            case PermissionType.kLocation:
              return loadTimeData.getBoolean(
                         'privacyHubLocationAccessControlEnabled') &&
                  this.getPref<GeolocationAccessLevel>(
                          'ash.user.geolocation_access_level')
                      .value !== GeolocationAccessLevel.ALLOWED;
            case PermissionType.kMicrophone:
              return !this.getPref('ash.user.microphone_allowed').value;
            case PermissionType.kContacts:
            case PermissionType.kStorage:
            case PermissionType.kNotifications:
            case PermissionType.kPrinting:
            case PermissionType.kFileHandling:
              return false;
            default:
              assertNotReached();
          }
        }
      }
      return PrivacyHubMixin;
    });

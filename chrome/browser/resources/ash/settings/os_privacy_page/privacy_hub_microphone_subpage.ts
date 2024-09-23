// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-microphone-subpage' contains a detailed overview about
 * the state of the system microphone access.
 */

import './privacy_hub_app_permission_row.js';
import './privacy_hub_system_service_row.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {isPermissionEnabled} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {MediaDevicesProxy} from '../common/media_devices_proxy.js';
import {App, AppPermissionsHandlerInterface, AppPermissionsObserverReceiver} from '../mojom-webui/app_permission_handler.mojom-webui.js';

import {getAppPermissionProvider} from './mojo_interface_provider.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {MICROPHONE_SUBPAGE_USER_ACTION_HISTOGRAM_NAME, NUMBER_OF_POSSIBLE_USER_ACTIONS, PrivacyHubSensorSubpageUserAction} from './privacy_hub_metrics_util.js';
import {getTemplate} from './privacy_hub_microphone_subpage.html.js';

/**
 * Whether the app has microphone permission defined.
 * */
function hasMicrophonePermission(app: App): boolean {
  return app.permissions[PermissionType.kMicrophone] !== undefined;
}

const SettingsPrivacyHubMicrophoneSubpageBase =
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsPrivacyHubMicrophoneSubpage extends
    SettingsPrivacyHubMicrophoneSubpageBase {
  static get is() {
    return 'settings-privacy-hub-microphone-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Apps with microphone permission.
       * Only contains apps that are displayed in the App Management page.
       * Does not contain system apps.
       */
      appList_: {
        type: Array,
        value: [],
      },

      systemApps_: {
        type: Array,
        value: [],
      },

      connectedMicrophoneNames_: {
        type: Array,
        value: [],
      },

      /**
       * Indicates whether `connectedMicrophoneNames_` is empty.
       */
      isMicListEmpty_: {
        type: Boolean,
        computed: 'computeIsMicListEmpty_(connectedMicrophoneNames_)',
      },

      /**
       * Indicates whether the microphone hardware toggle is active.
       */
      microphoneHardwareToggleActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the `cr-toggle` for microphone should be disabled.
       */
      shouldDisableMicrophoneToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableMicrophoneToggle_(isMicListEmpty_, ' +
            'microphoneHardwareToggleActive_)',
      },
    };
  }

  private appList_: App[];
  private appPermissionsObserverReceiver_: AppPermissionsObserverReceiver|null;
  private browserProxy_: PrivacyHubBrowserProxy;
  private connectedMicrophoneNames_: string[];
  private isMicListEmpty_: boolean;
  private microphoneHardwareToggleActive_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private shouldDisableMicrophoneToggle_: boolean;
  private systemApps_: App[];

  constructor() {
    super();

    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();

    this.mojoInterfaceProvider_ = getAppPermissionProvider();

    this.appPermissionsObserverReceiver_ = null;
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'microphone-hardware-toggle-changed', (enabled: boolean) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });
    this.browserProxy_.getInitialMicrophoneHardwareToggleState().then(
        (enabled) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });

    this.updateMicrophoneList_();
    MediaDevicesProxy.getMediaDevices().addEventListener(
        'devicechange', () => this.updateMicrophoneList_());
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.appPermissionsObserverReceiver_ =
        new AppPermissionsObserverReceiver(this);
    this.mojoInterfaceProvider_.addObserver(
        this.appPermissionsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.updateAppLists_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appPermissionsObserverReceiver_!.$.close();
  }

  private setMicrophoneHardwareToggleState_(enabled: boolean): void {
    this.microphoneHardwareToggleActive_ = enabled;
  }

  private async updateAppLists_(): Promise<void> {
    const apps = (await this.mojoInterfaceProvider_.getApps()).apps;
    this.appList_ = apps.filter(hasMicrophonePermission);

    this.systemApps_ =
        (await this.mojoInterfaceProvider_.getSystemAppsThatUseMicrophone())
            .apps;
  }

  private getSystemServicesPermissionText_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n('privacyHubSystemServicesBlockedText');
  }

  /**
   * The function is used for sorting app names alphabetically.
   */
  private alphabeticalSort_(first: App, second: App): number {
    return first.name!.localeCompare(second.name!);
  }

  private async updateMicrophoneList_(): Promise<void> {
    const connectedMicrophoneNames: string[] = [];
    const devices: MediaDeviceInfo[] =
        await MediaDevicesProxy.getMediaDevices().enumerateDevices();

    devices.forEach((device) => {
      if (device.kind === 'audioinput' && device.deviceId !== 'default') {
        connectedMicrophoneNames.push(device.label);
      }
    });

    this.connectedMicrophoneNames_ = connectedMicrophoneNames;
  }

  private computeIsMicListEmpty_(): boolean {
    return this.connectedMicrophoneNames_.length === 0;
  }

  private computeOnOffText_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ? this.i18n('deviceOn') : this.i18n('deviceOff');
  }

  private computeOnOffSubtext_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ?
        this.i18n('privacyHubMicrophoneSubpageMicrophoneToggleSubtext') :
        this.i18n('privacyHubMicrophoneAccessBlockedText');
  }

  private computeShouldDisableMicrophoneToggle_(): boolean {
    return this.microphoneHardwareToggleActive_ || this.isMicListEmpty_;
  }

  private onManagePermissionsInChromeRowClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        MICROPHONE_SUBPAGE_USER_ACTION_HISTOGRAM_NAME,
        PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED,
        NUMBER_OF_POSSIBLE_USER_ACTIONS);

    this.mojoInterfaceProvider_.openBrowserPermissionSettings(
        PermissionType.kMicrophone);
  }

  /**
   * Returns true if the microphone permission of the app is in Allowed or
   * equivalent state.
   */
  private isMicrophonePermissionEnabled_(app: App): boolean {
    assert(hasMicrophonePermission(app));
    return isPermissionEnabled(
        app.permissions[PermissionType.kMicrophone]!.value);
  }

  /** Implements AppPermissionsObserver.OnAppUpdated */
  onAppUpdated(updatedApp: App): void {
    if (!hasMicrophonePermission(updatedApp)) {
      return;
    }
    const idx = this.appList_.findIndex(app => app.id === updatedApp.id);
    if (idx === -1) {
      // New app installed.
      this.push('appList_', updatedApp);
    } else {
      // An already installed app is updated.
      this.splice('appList_', idx, 1, updatedApp);
    }
  }

  /** Implements AppPermissionsObserver.OnAppRemoved */
  onAppRemoved(appId: string): void {
    const idx = this.appList_.findIndex(app => app.id === appId);
    if (idx !== -1) {
      this.splice('appList_', idx, 1);
    }
  }

  private getMicrophoneToggle_(): CrToggleElement {
    return castExists(
        this.shadowRoot!.querySelector<CrToggleElement>('#microphoneToggle'));
  }

  private onAccessStatusRowClick_(): void {
    if (this.shouldDisableMicrophoneToggle_) {
      return;
    }

    this.getMicrophoneToggle_().click();
  }

  private onMicrophoneToggleClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        MICROPHONE_SUBPAGE_USER_ACTION_HISTOGRAM_NAME,
        PrivacyHubSensorSubpageUserAction.SYSTEM_ACCESS_CHANGED,
        NUMBER_OF_POSSIBLE_USER_ACTIONS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubMicrophoneSubpage.is]:
        SettingsPrivacyHubMicrophoneSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubMicrophoneSubpage.is,
    SettingsPrivacyHubMicrophoneSubpage);

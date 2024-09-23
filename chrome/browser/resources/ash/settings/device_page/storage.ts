// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import './storage_external.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isCrostiniSupported, isExternalStorageEnabled, isSkyVaultEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, StorageSpaceState} from './device_page_browser_proxy.js';
import {getTemplate} from './storage.html.js';

interface StorageSizeStat {
  availableSize: string;
  usedSize: string;
  usedRatio: number;
  spaceState: StorageSpaceState;
}

export interface SettingsStorageElement {
  $: {
    availableLabelArea: HTMLElement,
    browsingDataSize: CrLinkRowElement,
    inUseLabelArea: HTMLElement,
  };
}

const SettingsStorageElementBase =
    I18nMixin(PrefsMixin(RouteOriginMixin(WebUiListenerMixin(PolymerElement))));

export class SettingsStorageElement extends SettingsStorageElementBase {
  static get is() {
    return 'settings-storage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      showCrostiniStorage_: {
        type: Boolean,
        value: false,
      },

      isDriveEnabled_: {
        type: Boolean,
        value: true,
      },

      isEphemeralUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isCryptohomeDataEphemeral');
        },
      },

      isExternalStorageEnabled_: {
        type: Boolean,
        value: () => {
          return isExternalStorageEnabled();
        },
      },

      showOtherUsers_: {
        type: Boolean,
        // Initialize showOtherUsers_ to false if the user is ephemeral.
        value() {
          return !loadTimeData.getBoolean('isCryptohomeDataEphemeral');
        },
      },

      isSkyVaultEnabled_: {
        type: Boolean,
        value() {
          return isSkyVaultEnabled();
        },
      },

      /**
       * Sublabel for the MyFiles section, later it will be updated with the
       * calculated size.
       */
      myFilesSizeSubLabel_: {
        type: String,
        value(this: SettingsStorageElement) {
          return this.i18n('storageSizeComputing');
        },
      },

      /**
       * Sublabel for storage encryption label.
       */
      storageEncryptionSubLabel_: {
        type: String,
        value(this: SettingsStorageElement) {
          return this.i18n('storageSizeComputing');
        },
      },

      sizeStat_: Object,
    };
  }

  static get observers() {
    return [
      'handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)',
      'handleDriveDisabledChanged_(prefs.gdata.disabled.value)',
    ];
  }

  private browserProxy_: DevicePageBrowserProxy;
  private isEphemeralUser_: boolean;
  private showCrostiniStorage_: boolean;
  private isDriveEnabled_: boolean;
  private isExternalStorageEnabled_: boolean;
  private showOtherUsers_: boolean;
  private sizeStat_: StorageSizeStat;
  private updateTimerId_: number;
  private myFilesSizeSubLabel_: string;
  private storageEncryptionSubLabel_: string;
  private readonly isSkyVaultEnabled_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.STORAGE;

    /**
     * Timer ID for periodic update.
     */
    this.updateTimerId_ = -1;

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'storage-size-stat-changed',
        (sizeStat: StorageSizeStat) => this.handleSizeStatChanged_(sizeStat));
    this.addWebUiListener(
        'storage-my-files-size-changed',
        (size: string) => this.handleMyFilesSizeChanged_(size));
    this.addWebUiListener(
        'storage-browsing-data-size-changed',
        (size: string) => this.handleBrowsingDataSizeChanged_(size));
    this.addWebUiListener(
        'storage-apps-size-changed',
        (size: string) => this.handleAppsSizeChanged_(size));
    this.addWebUiListener(
        'storage-drive-offline-size-changed',
        (size: string) => this.handleDriveOfflineSizeChanged_(size));
    this.addWebUiListener(
        'storage-crostini-size-changed',
        (size: string) => this.handleCrostiniSizeChanged_(size));
    if (this.showOtherUsers_) {
      this.addWebUiListener(
          'storage-other-users-size-changed',
          (size: string, noOtherUsers: boolean) =>
              this.handleOtherUsersSizeChanged_(size, noOtherUsers));
      this.addWebUiListener(
          'storage-system-size-changed',
          (size: string) => this.handleSystemSizeChanged_(size));
    }
    if (!this.isEphemeralUser_) {
      this.browserProxy_.getStorageEncryptionInfo().then(
          encryptionInfo => {
            this.storageEncryptionSubLabel_ = encryptionInfo;
          },
          reason => {
            console.warn(`Unable to get info: ${reason}`);
          });
    }
  }

  override ready(): void {
    super.ready();

    const r = routes;
    this.addFocusConfig(r.CROSTINI_DETAILS, '#crostiniSize');
    this.addFocusConfig(r.ACCOUNTS, '#otherUsersSize');
    this.addFocusConfig(
        r.EXTERNAL_STORAGE_PREFERENCES, '#externalStoragePreferences');
    this.addFocusConfig(r.APP_MANAGEMENT, '#appsSize');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    if (newRoute !== this.route) {
      return;
    }
    this.onPageShown_();
  }

  private onPageShown_(): void {
    // Updating storage information can be expensive (e.g. computing directory
    // sizes recursively), so we delay this operation until the page is shown.
    this.browserProxy_.updateStorageInfo();
    // We update the storage usage periodically when the overlay is visible.
    this.startPeriodicUpdate_();
  }

  /**
   * Handler for tapping the MyFiles item.
   */
  private onMyFilesClick_(): void {
    if (this.localUserFilesAllowed_(
            this.getPref('filebrowser.local_user_files_allowed').value)) {
      this.browserProxy_.openMyFiles();
    }
  }

  /**
   * Handler for tapping the "Browsing data" item.
   */
  private onBrowsingDataClick_(): void {
    this.browserProxy_.openBrowsingDataSettings();
  }

  /**
   * Handler for tapping the "Apps and Extensions" item.
   */
  private onAppsClick_(): void {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  /**
   * Handler for tapping the "Offline files" item.
   */
  private onDriveOfflineClick_(): void {
    Router.getInstance().navigateTo(
        routes.GOOGLE_DRIVE,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  /**
   * Handler for tapping the "Linux storage" item.
   */
  private onCrostiniClick_(): void {
    Router.getInstance().navigateTo(
        routes.CROSTINI_DETAILS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  /**
   * Handler for tapping the "Other users" item.
   */
  private onOtherUsersClick_(): void {
    Router.getInstance().navigateTo(
        routes.ACCOUNTS,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  /**
   * Handler for tapping the "External storage preferences" item.
   */
  private onExternalStoragePreferencesClick_(): void {
    Router.getInstance().navigateTo(routes.EXTERNAL_STORAGE_PREFERENCES);
  }

  private handleSizeStatChanged_(sizeStat: StorageSizeStat): void {
    this.sizeStat_ = sizeStat;
    this.$.inUseLabelArea.style.width = (sizeStat.usedRatio * 100) + '%';
    this.$.availableLabelArea.style.width =
        ((1 - sizeStat.usedRatio) * 100) + '%';
  }

  /**
   * @param size Formatted string representing the size of MyFiles.
   */
  private handleMyFilesSizeChanged_(size: string): void {
    this.myFilesSizeSubLabel_ = size;
  }

  /**
   * @param size Formatted string representing the size of Browsing data.
   */
  private handleBrowsingDataSizeChanged_(size: string): void {
    this.$.browsingDataSize.subLabel = size;
  }

  /**
   * @param size Formatted string representing the size of Apps and
   *     extensions storage.
   */
  private handleAppsSizeChanged_(size: string): void {
    this.shadowRoot!.querySelector<CrLinkRowElement>('#appsSize')!.subLabel =
        size;
  }

  /**
   * @param size Formatted string representing the size of pinned files in
   *     Google Drive.
   */
  private handleDriveOfflineSizeChanged_(size: string): void {
    if (!this.shouldShowOfflineFilesRow_()) {
      return;
    }
    this.shadowRoot!.querySelector<CrLinkRowElement>(
                        '#driveOfflineSize')!.subLabel = size;
  }

  /**
   * @param size Formatted string representing the size of Crostini storage.
   */
  private handleCrostiniSizeChanged_(size: string): void {
    if (this.showCrostiniStorage_) {
      this.shadowRoot!.querySelector<CrLinkRowElement>(
                          '#crostiniSize')!.subLabel = size;
    }
  }

  /**
   * @param size Formatted string representing the size of Other users.
   * @param noOtherUsers True if there is no other registered users
   *     on the device.
   */
  private handleOtherUsersSizeChanged_(size: string, noOtherUsers: boolean):
      void {
    if (this.isEphemeralUser_ || noOtherUsers) {
      this.showOtherUsers_ = false;
      return;
    }
    this.showOtherUsers_ = true;
    this.shadowRoot!.querySelector<CrLinkRowElement>(
                        '#otherUsersSize')!.subLabel = size;
  }

  /**
   * @param size Formatted string representing the System size.
   */
  private handleSystemSizeChanged_(size: string): void {
    this.shadowRoot!.getElementById('systemSizeSubLabel')!.innerText = size;
  }

  /**
   * @param enabled True if Crostini is enabled.
   */
  private handleCrostiniEnabledChanged_(enabled: boolean): void {
    this.showCrostiniStorage_ = enabled && isCrostiniSupported();
  }

  /**
   * Handles showing or hiding the Offline files row if Drive is disabled.
   */
  private handleDriveDisabledChanged_(disabled: boolean): void {
    this.isDriveEnabled_ = !disabled;
  }

  /**
   * Whether to show the Offline files row or not.
   */
  private shouldShowOfflineFilesRow_(): boolean {
    return this.isDriveEnabled_;
  }

  /**
   * Starts periodic update for storage usage.
   */
  private startPeriodicUpdate_(): void {
    // We update the storage usage every 5 seconds.
    if (this.updateTimerId_ === -1) {
      this.updateTimerId_ = window.setInterval(() => {
        if (Router.getInstance().currentRoute !== routes.STORAGE) {
          this.stopPeriodicUpdate_();
          return;
        }
        this.browserProxy_.updateStorageInfo();
      }, 5000);
    }
  }

  /**
   * Stops periodic update for storage usage.
   */
  private stopPeriodicUpdate_(): void {
    if (this.updateTimerId_ !== -1) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = -1;
    }
  }

  /**
   * Returns true if the remaining space is low, but not critically low.
   * @param spaceState Status about the remaining space.
   */
  private isSpaceLow_(spaceState: StorageSpaceState): boolean {
    return spaceState === StorageSpaceState.LOW;
  }

  /**
   * Returns true if the remaining space is critically low.
   * @param spaceState Status about the remaining space.
   */
  private isSpaceCriticallyLow_(spaceState: StorageSpaceState): boolean {
    return spaceState === StorageSpaceState.CRITICALLY_LOW;
  }

  /**
   * Computes class name of the bar based on the remaining space size.
   * @param spaceState Status about the remaining space.
   */
  private getBarClass_(spaceState: StorageSpaceState): string {
    switch (spaceState) {
      case StorageSpaceState.LOW:
        return 'space-low';
      case StorageSpaceState.CRITICALLY_LOW:
        return 'space-critically-low';
      default:
        return '';
    }
  }

  private roundTo2DecimalPoints_(n: number): string {
    return n.toFixed(2);
  }

  /**
   * Checks feature flags and pref values to determine whether storing user
   * files locally is allowed.
   * @param prefValue The value of the local_user_files_allowed pref. Ignored if
   *     feature flags are disabled.
   * @returns Whether local user files are allowed.
   */
  private localUserFilesAllowed_(prefValue: boolean): boolean {
    // If SkyVault is disabled, we don't care about the pref value.
    return !this.isSkyVaultEnabled_ || prefValue;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-storage': SettingsStorageElement;
  }
}

customElements.define(SettingsStorageElement.is, SettingsStorageElement);

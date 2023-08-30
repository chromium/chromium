// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 */
import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './office_page.js';
import './smb_shares_page.js';
import '/shared/settings/controls/settings_toggle_button.js';

import {SmbBrowserProxy, SmbBrowserProxyImpl} from 'chrome://resources/ash/common/smb_shares/smb_browser_proxy.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {OneDriveBrowserProxy} from './one_drive_browser_proxy.js';
import {OneDriveConnectionState} from './one_drive_subpage.js';
import {getTemplate} from './os_files_page.html.js';

const OsSettingsFilesPageElementBase = WebUiListenerMixin(
    PrefsMixin(DeepLinkingMixin(RouteOriginMixin(I18nMixin(PolymerElement)))));

export class OsSettingsFilesPageElement extends OsSettingsFilesPageElementBase {
  static get is() {
    return 'os-settings-files-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kFiles,
        readOnly: true,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kGoogleDriveConnection]),
      },

      showOfficeSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOfficeSettings');
        },
      },

      showGoogleDriveSettingsPage_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showGoogleDriveSettingsPage') ||
            loadTimeData.getBoolean('enableDriveFsBulkPinning'),
      },

      isBulkPinningEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableDriveFsBulkPinning');
        },
      },

      /**
       * Indicates whether the user is connected to OneDrive or not.
       */
      oneDriveConnectionState_: {
        type: String,
        value() {
          return OneDriveConnectionState.LOADING;
        },
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      showAddSmbDialog_: {
        type: Boolean,
        value: false,
      },

      showAddSmbButton_: {
        type: Boolean,
        value: true,
      },
    };
  }

  /**
   * Observe the state of `prefs.gdata.disabled` if it gets changed from another
   * location (e.g. enterprise policy).
   */
  static get observers() {
    return [
      `updateDriveDisabled_(prefs.gdata.disabled.*)`,
      `updateBulkPinningPrefEnabled_(prefs.drivefs.bulk_pinning_enabled.*)`,
    ];
  }

  /**
   * Resolved once the async calls initiated by the constructor have resolved.
   */
  initPromise: Promise<void>;
  private driveDisabled_: boolean;
  private bulkPinningPrefEnabled_: boolean;
  private isBulkPinningEnabled_: boolean;
  private showOfficeSettings_: boolean;
  private oneDriveConnectionState_: string;
  private oneDriveEmailAddress_: string|null;
  private oneDriveProxy_: OneDriveBrowserProxy;
  private osFilesPageProxy_: SmbBrowserProxy;
  private section_: Section;
  private isRevampWayfindingEnabled_: boolean;
  private showAddSmbDialog_: boolean;
  private showAddSmbButton_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.FILES;

    if (this.showOfficeSettings_) {
      this.oneDriveProxy_ = OneDriveBrowserProxy.getInstance();
      this.initPromise = this.updateOneDriveEmail_();
    }

    this.osFilesPageProxy_ = SmbBrowserProxyImpl.getInstance();
  }

  override currentRouteChanged(route: Route, oldRoute?: Route) {
    super.currentRouteChanged(route, oldRoute);

    // Does not apply to this page.
    if (route !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns the browser proxy page handler for operations related to OneDrive
   * (to invoke functions).
   */
  get oneDrivePageHandler() {
    return this.oneDriveProxy_.handler;
  }

  /**
   * Returns the browser proxy callback router (to receive async messages).
   */
  get oneDriveCallbackRouter() {
    return this.oneDriveProxy_.observer;
  }

  override connectedCallback() {
    super.connectedCallback();
    if (this.showOfficeSettings_) {
      this.oneDriveCallbackRouter.onODFSMountOrUnmount.addListener(
          this.updateOneDriveEmail_.bind(this));
    }
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.SMB_SHARES, '#smbSharesRow');

    this.addEventListener(
        'smb-successfully-mounted-once',
        this.smbSuccessfullyMountedOnce.bind(this));

    // Only show the button if there has not been a SMB mounted before.
    this.osFilesPageProxy_.hasAnySmbMountedBefore().then(
        (hasMountedBefore: boolean) => {
          this.showAddSmbButton_ = !hasMountedBefore;
        });
  }

  private onTapSmbShares_() {
    Router.getInstance().navigateTo(routes.SMB_SHARES);
  }

  private onGoogleDrive_() {
    Router.getInstance().navigateTo(routes.GOOGLE_DRIVE);
  }

  private onTapOneDrive_() {
    Router.getInstance().navigateTo(routes.ONE_DRIVE);
  }

  private onTapOffice_() {
    Router.getInstance().navigateTo(routes.OFFICE);
  }

  private updateDriveDisabled_() {
    const disabled = this.getPref('gdata.disabled').value;
    this.driveDisabled_ = disabled;
  }

  private updateBulkPinningPrefEnabled_(): void {
    const enabled = this.getPref('drivefs.bulk_pinning_enabled').value;
    this.bulkPinningPrefEnabled_ = enabled;
  }

  private googleDriveSublabel_(): string {
    if (this.driveDisabled_) {
      return this.i18n('googleDriveNotSignedInSublabel');
    }

    return (this.isBulkPinningEnabled_ && this.bulkPinningPrefEnabled_) ?
        this.i18n('googleDriveFileSyncOnSublabel') :
        this.i18n('googleDriveSignedInAs');
  }

  private async updateOneDriveEmail_() {
    this.oneDriveConnectionState_ = OneDriveConnectionState.LOADING;
    const {email} = await this.oneDrivePageHandler.getUserEmailAddress();
    this.oneDriveEmailAddress_ = email;
    this.oneDriveConnectionState_ = email === null ?
        OneDriveConnectionState.DISCONNECTED :
        OneDriveConnectionState.CONNECTED;
  }

  private oneDriveSignedInLabel_(oneDriveConnectionState: string) {
    switch (oneDriveConnectionState) {
      case OneDriveConnectionState.CONNECTED:
        assert(this.oneDriveEmailAddress_);
        return this.i18n('oneDriveSignedInAs', this.oneDriveEmailAddress_);
      case OneDriveConnectionState.DISCONNECTED:
        return this.i18n('oneDriveDisconnected');
      default:
        return '';
    }
  }

  private getSmbSublabel_(): string|null {
    return this.isRevampWayfindingEnabled_ ?
        this.i18n('smbSharesTitleDescription') :
        null;
  }

  private onAddShareClick_(): void {
    this.showAddSmbDialog_ = true;
  }

  private onAddSmbDialogClosed_(): void {
    this.showAddSmbDialog_ = false;
  }

  private computeShowSmbButton_(): boolean {
    return this.isRevampWayfindingEnabled_ && this.showAddSmbButton_;
  }

  private computeShowSmbLinkRow_(): boolean {
    return !this.isRevampWayfindingEnabled_ ||
        (this.isRevampWayfindingEnabled_ && !this.showAddSmbButton_);
  }

  private smbSuccessfullyMountedOnce(): void {
    // Do not show SMB button on the Files page if an SMB mounts.
    this.showAddSmbButton_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsFilesPageElement.is]: OsSettingsFilesPageElement;
  }
}

customElements.define(
    OsSettingsFilesPageElement.is, OsSettingsFilesPageElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'files-settings-card' is the card element containing files settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {SmbBrowserProxy, SmbBrowserProxyImpl} from 'chrome://resources/ash/common/smb_shares/smb_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExhaustive} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './files_settings_card.html.js';
import {OneDriveBrowserProxy} from './one_drive_browser_proxy.js';
import {OneDriveConnectionState} from './one_drive_subpage.js';

const FilesSettingsCardElementBase =
    RouteOriginMixin(I18nMixin(DeepLinkingMixin(PrefsMixin(PolymerElement))));

export class FilesSettingsCardElement extends FilesSettingsCardElementBase {
  static get is() {
    return 'files-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([]),
      },

      bulkPinningPrefEnabled_: Boolean,
      mirrorSyncPrefEnabled_: Boolean,

      driveDisabled_: Boolean,

      isBulkPinningEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('enableDriveFsBulkPinning');
        },
        readOnly: true,
      },

      isMirrorSyncEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('enableDriveFsMirrorSync');
        },
        readOnly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      /**
       * Indicates whether the user is connected to OneDrive or not.
       */
      oneDriveConnectionState_: {
        type: String,
        value: () => {
          return OneDriveConnectionState.LOADING;
        },
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              googleDrive: 'os-settings:google-drive-revamp',
              ms365: 'os-settings:ms365',
              oneDrive: 'settings20:onedrive',
              smbShares: 'os-settings:folder-shared',
            };
          }

          return {
            googleDrive: 'os-settings:google-drive',
            ms365: '',
            oneDrive: 'settings20:onedrive',
            smbShares: '',
          };
        },
      },

      shouldShowOneDriveSettings_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('showOneDriveSettings');
        },
        readOnly: true,
      },

      shouldShowOfficeSettings_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('showOfficeSettings');
        },
        readOnly: true,
      },

      shouldShowAddSmbButton_: {
        type: Boolean,
        value: false,
      },

      shouldShowAddSmbDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      /**
       * Observe the state of `prefs.gdata.disabled` if it gets changed from
       * another location (e.g. enterprise policy).
       */
      'updateDriveDisabled_(prefs.gdata.disabled.value)',
      'updateBulkPinningPrefEnabled_(prefs.drivefs.bulk_pinning_enabled.value)',
      'updateMirrorSyncPrefEnabled_(prefs.drivefs.enable_mirror_sync.value)',
    ];
  }

  private bulkPinningPrefEnabled_: boolean;
  private mirrorSyncPrefEnabled_: boolean;
  private driveDisabled_: boolean;
  private isBulkPinningEnabled_: boolean;
  private isMirrorSyncEnabled_: boolean;
  private readonly isRevampWayfindingEnabled_: boolean;
  private oneDriveBrowserProxy_: OneDriveBrowserProxy|undefined;
  private oneDriveConnectionState_: OneDriveConnectionState;
  private oneDriveEmailAddress_: string|null;
  private rowIcons_: Record<string, string>;
  private smbBrowserProxy_: SmbBrowserProxy;
  private shouldShowAddSmbButton_: boolean;
  private shouldShowAddSmbDialog_: boolean;
  private shouldShowOneDriveSettings_: boolean;
  private shouldShowOfficeSettings_: boolean;


  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = this.isRevampWayfindingEnabled_ ? routes.SYSTEM_PREFERENCES :
                                                   routes.FILES;

    this.smbBrowserProxy_ = SmbBrowserProxyImpl.getInstance();

    if (this.shouldShowOneDriveSettings_) {
      this.oneDriveBrowserProxy_ = OneDriveBrowserProxy.getInstance();
    }
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (this.shouldShowOneDriveSettings_) {
      this.updateOneDriveEmail_();
      this.oneDriveBrowserProxy_!.observer.onODFSMountOrUnmount.addListener(
          this.updateOneDriveEmail_.bind(this));
    }
  }

  override ready(): void {
    super.ready();

    this.addEventListener(
        'smb-successfully-mounted-once',
        this.smbSuccessfullyMountedOnce_.bind(this));

    this.smbBrowserProxy_.hasAnySmbMountedBefore().then(
        (hasMountedBefore: boolean) => {
          this.shouldShowAddSmbButton_ = !hasMountedBefore;
        });

    this.addFocusConfig(routes.GOOGLE_DRIVE, '#googleDriveRow');
    this.addFocusConfig(routes.ONE_DRIVE, '#oneDriveRow');
    this.addFocusConfig(routes.OFFICE, '#officeRow');
    this.addFocusConfig(routes.SMB_SHARES, '#smbSharesRow');
  }

  override currentRouteChanged(route: Route, oldRoute?: Route): void {
    super.currentRouteChanged(route, oldRoute);

    // Does not apply to this page.
    if (route !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  updateOneDriveConnectionStateForTesting(oneDriveConnectionState:
                                              OneDriveConnectionState): void {
    this.oneDriveConnectionState_ = oneDriveConnectionState;
  }

  private updateDriveDisabled_(disabled: boolean): void {
    this.driveDisabled_ = disabled;
  }

  private updateBulkPinningPrefEnabled_(enabled: boolean): void {
    this.bulkPinningPrefEnabled_ = enabled;
  }

  private updateMirrorSyncPrefEnabled_(enabled: boolean): void {
    this.mirrorSyncPrefEnabled_ = enabled;
  }

  private getGoogleDriveSubLabelInnerHtml_(): TrustedHTML {
    if (this.driveDisabled_) {
      return this.i18nAdvanced('googleDriveNotSignedInSublabel');
    }

    return ((this.isBulkPinningEnabled_ && this.bulkPinningPrefEnabled_) ||
            (this.isMirrorSyncEnabled_ && this.mirrorSyncPrefEnabled_)) ?
        this.i18nAdvanced('googleDriveFileSyncOnSublabel') :
        this.i18nAdvanced('googleDriveSignedInAs', {attrs: ['id']});
  }

  private computeOneDriveSignedInLabel_(): string {
    switch (this.oneDriveConnectionState_) {
      case OneDriveConnectionState.CONNECTED:
        assert(this.oneDriveEmailAddress_);
        return this.i18n('oneDriveSignedInAs', this.oneDriveEmailAddress_);
      case OneDriveConnectionState.DISCONNECTED:
        return this.i18n('oneDriveDisconnected');
      case OneDriveConnectionState.LOADING:
        return this.i18n('oneDriveLoading');
      default:
        assertExhaustive(this.oneDriveConnectionState_);
    }
  }

  private computeShowSmbLinkRow_(): boolean {
    if (!this.isRevampWayfindingEnabled_) {
      return true;
    }

    return !this.shouldShowAddSmbButton_;
  }

  private async updateOneDriveEmail_(): Promise<void> {
    this.oneDriveConnectionState_ = OneDriveConnectionState.LOADING;
    const {email} =
        await this.oneDriveBrowserProxy_!.handler.getUserEmailAddress();
    this.oneDriveEmailAddress_ = email;
    this.oneDriveConnectionState_ = email === null ?
        OneDriveConnectionState.DISCONNECTED :
        OneDriveConnectionState.CONNECTED;
  }

  private onClickGoogleDrive_(): void {
    Router.getInstance().navigateTo(routes.GOOGLE_DRIVE);
  }

  private onClickOneDrive_(): void {
    Router.getInstance().navigateTo(routes.ONE_DRIVE);
  }

  private onClickOffice_(): void {
    Router.getInstance().navigateTo(routes.OFFICE);
  }

  private onClickSmbShares_(): void {
    Router.getInstance().navigateTo(routes.SMB_SHARES);
  }

  private openAddSmbDialog_(): void {
    this.shouldShowAddSmbDialog_ = true;
  }

  private closeAddSmbDialog_(): void {
    this.shouldShowAddSmbDialog_ = false;
  }

  private smbSuccessfullyMountedOnce_(): void {
    // Do not show SMB button on the Files page if an SMB mounts.
    this.shouldShowAddSmbButton_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FilesSettingsCardElement.is]: FilesSettingsCardElement;
  }
}

customElements.define(FilesSettingsCardElement.is, FilesSettingsCardElement);

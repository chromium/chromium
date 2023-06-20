// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_section.js';
import '../os_settings_page/os_settings_subpage.js';
import '../settings_shared.css.js';
import './office_page.js';
import './smb_shares_page.js';
import '/shared/settings/controls/settings_toggle_button.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {OneDriveBrowserProxy} from './one_drive_browser_proxy.js';
import {OneDriveConnectionState} from './one_drive_subpage.js';
import {getTemplate} from './os_files_page.html.js';

const OsSettingsFilesPageElementBase =
    PrefsMixin(DeepLinkingMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

export class OsSettingsFilesPageElement extends OsSettingsFilesPageElementBase {
  /**
   * Resolved once the async calls initiated by the constructor have resolved.
   */
  initPromise: Promise<void>;

  constructor() {
    super();
    if (this.showOfficeSettings_) {
      this.oneDriveProxy_ = OneDriveBrowserProxy.getInstance();
      this.initPromise = this.updateOneDriveEmail_();
    }
  }
  static get is() {
    return 'os-settings-files-page';
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

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SMB_SHARES) {
            map.set(routes.SMB_SHARES.path, '#smbShares');
          }
          return map;
        },
      },

      /** @private */
      showOfficeSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOfficeSettings');
        },
      },

      /** @private */
      isBulkPinningEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableDriveFsBulkPinning');
        },
      },

      /**
         @private Indicates whether the user is connected to OneDrive or not.
       */
      oneDriveConnectionState_: {
        type: String,
        value() {
          return OneDriveConnectionState.LOADING;
        },
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
    ];
  }

  private driveDisabled_: boolean;
  private showOfficeSettings_: boolean;
  private focusConfig_: Map<string, string>;
  private oneDriveConnectionState_: string;
  private oneDriveEmailAddress_: string|null;
  private oneDriveProxy_: OneDriveBrowserProxy;
  private section_: Section;

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.FILES) {
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

  private googleDriveSignedInLabel_(): string {
    return this.driveDisabled_ ? this.i18n('googleDriveDisabledLabel') :
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
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-files-page': OsSettingsFilesPageElement;
  }
}

customElements.define(
    OsSettingsFilesPageElement.is, OsSettingsFilesPageElement);

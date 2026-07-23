// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

enum UpdateState {
  IDLE = 'idle',
  CHECKING = 'checking',
  UP_TO_DATE = 'up-to-date',
  UPDATE_AVAILABLE = 'update-available',
}
import './app_management_cros_shared_style.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, browserProxyFactory, InstallReason, InstallSource} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';

import {getTemplate} from './app_details_item.html.js';

const AppManagementAppDetailsItemBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

export class AppManagementAppDetailsItem extends
    AppManagementAppDetailsItemBase {
  static get is() {
    return 'app-management-app-details-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: {
        type: Object,
      },

      appId_: {
        type: String,
        observer: 'appIdChanged_',
      },

      apps_: {
        type: Object,
      },

      subAppToParentAppId_: {
        type: Object,
      },

      updateState_: {
        type: String,
        value: UpdateState.IDLE,
      },

      availableUpdateVersion_: {
        type: String,
        value: '',
      },

      hasOpenWindows_: {
        type: Boolean,
        value: false,
      },

      showUpdateFoundDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    this.hasOpenWindows_ = false;
    this.showUpdateFoundDialog_ = false;
  }

  declare app: App;
  declare private appId_: string;
  declare private apps_: Record<string, App>;
  declare private subAppToParentAppId_: Record<string, string>;
  declare private updateState_: UpdateState;
  declare private availableUpdateVersion_: string;
  declare private hasOpenWindows_: boolean;
  declare private showUpdateFoundDialog_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch('appId_', state => state.selectedAppId);
    this.watch('apps_', state => state.apps);
    this.watch('subAppToParentAppId_', state => state.subAppToParentAppId);
    this.updateFromStore();
  }

  private resetUpdateState_(): void {
    this.updateState_ = UpdateState.IDLE;
    this.availableUpdateVersion_ = '';
    this.showUpdateFoundDialog_ = false;
    this.hasOpenWindows_ = false;
  }

  private appIdChanged_(appId: string): void {
    if (appId && this.app) {
      browserProxyFactory.getInstance().handler.updateAppSize(appId);
      this.resetUpdateState_();
    }
  }

  /**
   * The version is shown for every app apart from System apps.
   */
  private shouldShowVersion_(app: App): boolean {
    if (app.installReason === InstallReason.kSystem) {
      return false;
    }
    if (this.isIsolatedWebApp_(app)) {
      return false;
    }
    return Boolean(app.version);
  }

  private isIsolatedWebApp_(app: App): boolean {
    return loadTimeData.getBoolean('isIwaInlineUpdateEnabled') && app &&
        app.type === AppType.kWeb &&
        app.publisherId.startsWith('isolated-app://');
  }

  private isChecking_(state: UpdateState): boolean {
    return state === UpdateState.CHECKING;
  }

  private onCheckUpdateButtonClick_(): void {
    this.checkForUpdates_();
  }

  private onUpdateFoundDialogClose_(): void {
    this.showUpdateFoundDialog_ = false;
  }

  private onUpdateFoundDialogCancel_(): void {
    this.showUpdateFoundDialog_ = false;
  }

  private onUpdateFoundDialogConfirm_(): void {
    this.showUpdateFoundDialog_ = false;
    this.triggerUpdate_();
  }

  private async checkForUpdates_(): Promise<void> {
    if (!this.app) {
      return;
    }
    const targetAppId = this.app.id;
    this.updateState_ = UpdateState.CHECKING;
    try {
      const response = await browserProxyFactory.getInstance()
                           .handler.checkForIsolatedWebAppUpdate(targetAppId);
      if (this.app?.id !== targetAppId) {
        return;
      }
      if (response && response.updateVersion) {
        this.availableUpdateVersion_ =
            response.updateVersion.components.join('.');
        this.updateState_ = UpdateState.UPDATE_AVAILABLE;

        // Query open windows state to configure dialogue body
        const numWindowsResponse =
            await browserProxyFactory.getInstance().handler.getNumWindowsForApp(
                targetAppId);
        if (this.app?.id !== targetAppId) {
          return;
        }
        this.hasOpenWindows_ =
            numWindowsResponse ? (numWindowsResponse.numWindows > 0) : false;
        this.showUpdateFoundDialog_ = true;
      } else {
        this.updateState_ = UpdateState.UP_TO_DATE;
      }
    } catch (e) {
      if (this.app?.id === targetAppId) {
        console.error('Failed to check for updates:', e);
        this.resetUpdateState_();
      }
    }
  }

  private async triggerUpdate_(): Promise<void> {
    if (!this.app) {
      return;
    }
    const targetAppId = this.app.id;
    this.updateState_ = UpdateState.CHECKING;
    try {
      const response = await browserProxyFactory.getInstance()
                           .handler.applyIsolatedWebAppUpdate(targetAppId);
      if (this.app?.id !== targetAppId) {
        return;
      }
      if (response && response.success) {
        this.resetUpdateState_();
      } else {
        this.updateState_ = UpdateState.UPDATE_AVAILABLE;
      }
    } catch (e) {
      if (this.app?.id === targetAppId) {
        console.error('Failed to apply update:', e);
        this.updateState_ = UpdateState.UPDATE_AVAILABLE;
      }
    }
  }

  private getUpdateStatusString_(state: UpdateState): string {
    if (state === UpdateState.CHECKING) {
      return this.i18n('appManagementCheckingForUpdates');
    }
    if (state === UpdateState.UP_TO_DATE) {
      return this.i18n('appManagementAppIsUpToDate');
    }
    return '';
  }

  private getUpdateFoundDialogBody_(
      hasOpenWindows: boolean, version: string, title: string): string {
    if (hasOpenWindows) {
      return this.i18n(
          'appManagementUpdateFoundWarningDialogDescription', version, title);
    }
    return this.i18n(
        'appManagementUpdateFoundDialogDescription', version, title);
  }

  /**
   * Storage information is shown for every app apart from System apps.
   */
  private shouldShowStorage_(app: App): boolean {
    if (app.installReason === InstallReason.kSystem) {
      return false;
    }
    return true;
  }

  private shouldShowAppSize_(app: App): boolean {
    return Boolean(app.appSize);
  }

  private shouldShowDataSize_(app: App): boolean {
    return Boolean(app.dataSize);
  }

  private shouldShowInfoIcon_(app: App): boolean {
    return app.type === AppType.kWeb &&
        (app.installSource === InstallSource.kBrowser ||
         app.installSource === InstallSource.kSync);
  }

  /**
   * The launch icon is show for apps installed from the Chrome Web
   * Store and Google Play Store.
   */
  private shouldShowLaunchIcon_(app: App): boolean {
    return app.installSource === InstallSource.kChromeWebStore ||
        app.installSource === InstallSource.kPlayStore;
  }

  private getTypeString_(app: App, suffix: string = ''): string {
    // When installReason = kSystem, the system has determined that the app
    // needs to be installed. This includes apps such as Chrome and the Play
    // Store.
    if (app.installReason === InstallReason.kSystem) {
      return this.i18n('appManagementAppDetailsTypeCrosSystem');
    }
    switch (app.type) {
      case AppType.kArc:
        return this.i18n('appManagementAppDetailsTypeAndroid' + suffix);
      case AppType.kChromeApp:
        return this.i18n('appManagementAppDetailsTypeChrome' + suffix);
      case AppType.kWeb:
      case AppType.kExtension:
        return this.i18n('appManagementAppDetailsTypeWeb' + suffix);
      default:
        console.error('App type not handled by app management.');
        return '';
    }
  }

  private getInstallSourceString_(app: App): string {
    switch (app.installSource) {
      case InstallSource.kChromeWebStore:
        return this.i18n('appManagementAppDetailsInstallSourceWebStore');
      case InstallSource.kPlayStore:
        return this.i18n('appManagementAppDetailsInstallSourcePlayStore');
      default:
        console.error('Install source not recognised.');
        return '';
    }
  }

  private getTypeAndSourceString_(app: App): string {
    if (app.installReason === InstallReason.kPolicy) {
      return this.getTypeString_(app, 'InstallReasonPolicy');
    }
    if (app.type === AppType.kWeb &&
        (app.installSource === InstallSource.kBrowser ||
         app.installSource === InstallSource.kSync)) {
      return this.i18n('appManagementAppDetailsInstallSourceBrowser');
    }
    if (app.installSource === InstallSource.kPlayStore ||
        app.installSource === InstallSource.kChromeWebStore) {
      return this
          .i18nAdvanced('appManagementAppDetailsTypeAndSourceCombined', {
            substitutions: [
              this.getTypeString_(app),
              this.getInstallSourceString_(app),
            ],
          })
          .toString();
    }
    if (app.installSource === InstallSource.kSystem) {
      return this
          .i18nAdvanced('appManagementAppDetailsTypeAndSourcePreinstalledApp', {
            substitutions: [
              this.getTypeString_(app),
              loadTimeData.getString('appManagementDeviceName'),
            ],
          })
          .toString();
    }
    return this.getTypeString_(app);
  }

  private onStoreLinkClicked_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    if (e.detail.event) {  // When the store link is clicked
      e.detail.event.preventDefault();
      e.stopPropagation();
    }

    if (this.app !== null) {
      recordAppManagementUserAction(
          this.app.type, AppManagementUserAction.APP_STORE_LINK_CLICKED);
      browserProxyFactory.getInstance().handler.openStorePage(this.app.id);
    }
  }

  /**
   * Returns the sanitized URL for apps downloaded from the Chrome browser, to
   * be shown in the tooltip.
   */
  private getTooltipText_(app: App): string {
    switch (app.installSource) {
      case InstallSource.kBrowser:
      case InstallSource.kSync:
        return app.publisherId.replace(/\?.*$/g, '');
      default:
        return '';
    }
  }

  private getTooltipA11yText_(app: App): string {
    return this.i18n(
        'appManagementAppDetailsTooltipWebA11y', this.getTooltipText_(app));
  }

  private getParentApp_(
      app: App|undefined, apps: Record<string, App>|undefined,
      subAppToParentAppId: Record<string, string>|undefined): App|null {
    if (!app || !subAppToParentAppId || !apps) {
      return null;
    }
    const parentId = subAppToParentAppId[app.id];
    if (!parentId) {
      return null;
    }
    return apps[parentId] || null;
  }

  private shouldShowSubappDataSharingExplanation_(
      app: App|undefined, apps: Record<string, App>|undefined,
      subAppToParentAppId: Record<string, string>|undefined): boolean {
    if (!app) {
      return false;
    }
    return app.type === AppType.kWeb &&
        this.getParentApp_(app, apps, subAppToParentAppId) !== null;
  }

  private getSubappDataSharingExplanationString_(
      app: App|undefined, apps: Record<string, App>|undefined,
      subAppToParentAppId: Record<string, string>|undefined): string {
    if (!app) {
      return '';
    }
    const parentApp = this.getParentApp_(app, apps, subAppToParentAppId);
    const parentAppName = parentApp ? (parentApp.title || '') : '';
    return this.i18n(
        'appManagementAppDetailsSubappDataSharingExplanation', parentAppName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-details-item': AppManagementAppDetailsItem;
  }
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);

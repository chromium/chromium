// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './web_app_internals.mojom-webui.js';
import type {BrowserProxy, IwaDevModeAppInfo, IwaDevModeLocation, VersionEntry} from './web_app_internals.mojom-webui.js';
import type {AppIndexEntry, DebugData} from './web_app_internals_utils.js';
import {debugDataJsonReplacer, filterToApp, getAppIndexEntries, getQuery} from './web_app_internals_utils.js';

export interface WebAppInternalsAppElement {
  $: {
    pinnedVersion: HTMLInputElement,
    pinnedVersionInputDialog: HTMLDialogElement,
    switchChannelInputDialog: HTMLDialogElement,
    updateChannel: HTMLInputElement,
    updateManifestDialog: HTMLDialogElement,
    updateManifestVersionSelect: HTMLSelectElement,
  };
}

export interface DevModeAppInfo extends IwaDevModeAppInfo {
  updateMsg?: string;
  isUpdating?: boolean;
  isDeleting?: boolean;
}

function originToText(origin: Origin): string {
  const shouldShowPort = !(origin.scheme === 'https' && origin.port === 443) &&
      !(origin.scheme === 'http' && origin.port === 80);
  const portString = shouldShowPort ? `:${origin.port}` : '';
  return `${origin.scheme}://${origin.host}${portString}`;
}

function filePathToText(filePath: FilePath): string {
  if (typeof filePath.path === 'string') {
    return filePath.path;
  }

  const decoder = new TextDecoder('utf-16');
  const buffer = new Uint16Array(filePath.path);
  return decoder.decode(buffer);
}

function formatDevModeLocation(location: IwaDevModeLocation): string {
  if (location.proxyOrigin) {
    return originToText(location.proxyOrigin);
  }
  if (location.bundlePath) {
    return filePathToText(location.bundlePath);
  }
  assertNotReached();
}

function compareStringVersions(v1: string, v2: string): number {
  const parts1 = v1.split('.').map(Number);
  const parts2 = v2.split('.').map(Number);

  for (let i = 0; i < Math.max(parts1.length, parts2.length); i++) {
    const part1 = parts1[i] || 0;
    const part2 = parts2[i] || 0;

    if (part1 < part2) {
      return -1;
    } else if (part1 > part2) {
      return 1;
    }
  }

  return 0;
}

export class WebAppInternalsAppElement extends CrLitElement {
  static get is() {
    return 'web-app-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      rawDebugData_: {type: String},
      debugInfoJson_: {type: String},
      parsedDebugData_: {type: Object},
      query_: {type: String},
      isIwaPolicyInstallEnabled_: {type: Boolean},
      isIwaDevModeEnabled_: {type: Boolean},
      iwaUpdatesMessage_: {type: String},
      iwaDevInstallProxyUrl_: {type: String},
      iwaDevInstallMessage_: {type: String},
      iwaDevUpdateManifestUrl_: {type: String},
      isInstallingProxy_: {type: Boolean},
      devModeApps_: {type: Array},
      devModeUpdatesMessage_: {type: String},
      manifestVersions_: {type: Array},
    };
  }

  protected accessor rawDebugData_: string = '';
  protected accessor debugInfoJson_: string = '';
  protected accessor parsedDebugData_: DebugData|null = null;
  protected accessor query_: string = '';
  protected accessor isIwaPolicyInstallEnabled_: boolean =
      loadTimeData.getBoolean('isIwaPolicyInstallEnabled');
  protected accessor isIwaDevModeEnabled_: boolean =
      loadTimeData.getBoolean('isIwaDevModeEnabled');
  protected accessor iwaUpdatesMessage_: string = '';
  protected accessor iwaDevInstallProxyUrl_: string = '';
  protected accessor iwaDevInstallMessage_: string = '';
  protected accessor iwaDevUpdateManifestUrl_: string = '';
  protected accessor isInstallingProxy_: boolean = false;
  protected accessor devModeApps_: DevModeAppInfo[] = [];
  protected accessor devModeUpdatesMessage_: string = '';
  protected accessor manifestVersions_: VersionEntry[] = [];

  private selectedManifestVersions_: VersionEntry[] = [];
  private selectedManifestUrl_: string = '';
  private activeAppId_: string = '';
  private activeAppName_: string = '';
  private tracker_: EventTracker = new EventTracker();
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.isIwaDevModeEnabled_) {
      this.refreshDevModeAppList_();
    }

    this.query_ = getQuery();
    this.tracker_.add(window, 'hashchange', () => {
      this.query_ = getQuery();
    });

    this.fetchDebugInfo_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('parsedDebugData_') ||
        changedPrivateProperties.has('rawDebugData_')) {
      this.debugInfoJson_ = this.parsedDebugData_ ?
          JSON.stringify(this.parsedDebugData_, debugDataJsonReplacer, 2) :
          this.rawDebugData_;
    }
  }

  private async fetchDebugInfo_(): Promise<void> {
    try {
      const response =
          await this.browserProxy_.handler.getDebugInfoAsJsonString();
      this.rawDebugData_ = response.result;
      try {
        this.parsedDebugData_ = JSON.parse(response.result);
      } catch {
        this.parsedDebugData_ = null;
      }
    } catch (e) {
      console.warn('Failed to fetch debug info', e);
    }
  }

  protected getAppIndexEntries_(): AppIndexEntry[] {
    if (!this.parsedDebugData_) {
      return [];
    }
    return getAppIndexEntries(this.parsedDebugData_, this.query_);
  }

  protected getFormattedJson_(): string {
    if (!this.parsedDebugData_) {
      return this.debugInfoJson_;
    }
    if (this.query_) {
      const displayData = filterToApp(this.parsedDebugData_, this.query_);
      return JSON.stringify(displayData, debugDataJsonReplacer, 2);
    }
    return this.debugInfoJson_;
  }

  protected onDownloadButtonClick_() {
    const url = URL.createObjectURL(new Blob([this.debugInfoJson_], {
      type: 'application/json',
    }));

    const a = document.createElement('a');
    a.href = url;
    a.download = 'web_app_internals.json';
    a.click();
    URL.revokeObjectURL(url);
  }

  protected onCopyButtonClick_(): void {
    navigator.clipboard.writeText(this.debugInfoJson_);
  }

  protected async onIwaUpdatesSearchButtonClick_() {
    this.iwaUpdatesMessage_ = 'Queueing update discovery tasks...';
    const response =
        await this.browserProxy_.handler.searchForIsolatedWebAppUpdates();
    this.iwaUpdatesMessage_ = response.result;
  }

  protected onIwaDevInstallProxyUrlInput_(e: Event) {
    this.iwaDevInstallProxyUrl_ = (e.target as HTMLInputElement).value;
  }

  protected onIwaDevInstallProxyUrlKeyup_(e: KeyboardEvent) {
    if (e.key !== 'Enter') {
      return;
    }
    e.preventDefault();
    this.installFromDevProxy_();
  }

  protected onIwaDevInstallProxyButtonClick_() {
    this.installFromDevProxy_();
  }

  private async installFromDevProxy_(): Promise<void> {
    const urlStr = this.iwaDevInstallProxyUrl_;
    if (!urlStr || this.isInstallingProxy_) {
      return;
    }

    let valid = false;
    try {
      const url = new URL(urlStr);
      valid = url.protocol === 'http:' || url.protocol === 'https:';
    } catch (_) {
      // Fall-through.
    }
    if (!valid) {
      this.iwaDevInstallMessage_ =
          `Installing IWA: ${urlStr} is not a valid URL`;
      return;
    }

    this.isInstallingProxy_ = true;
    this.iwaDevInstallMessage_ = `Installing IWA: ${urlStr}...`;

    try {
      const location: Url = urlStr;
      const {result} =
          await this.browserProxy_.handler.installIsolatedWebAppFromDevProxy(
              location);
      if (result.success) {
        this.iwaDevInstallMessage_ =
            `Installing IWA: ${urlStr} successfully installed.`;
        this.iwaDevInstallProxyUrl_ = '';
        this.refreshDevModeAppList_();
        return;
      }

      this.iwaDevInstallMessage_ =
          `Installing IWA: ${urlStr} failed to install: ${result.error}`;
    } finally {
      this.isInstallingProxy_ = false;
    }
  }

  protected async onIwaDevInstallBundleSelectorClick_() {
    this.iwaDevInstallMessage_ = 'Installing IWA from bundle...';

    const {result} = await this.browserProxy_.handler
                         .selectFileAndInstallIsolatedWebAppFromDevBundle();
    if (result.success) {
      this.iwaDevInstallMessage_ =
          `Installing IWA: successfully installed (Web Bundle ID: ${
              result.success.webBundleId}).`;
      this.refreshDevModeAppList_();
      return;
    }

    this.iwaDevInstallMessage_ =
        `Installing IWA: failed to install: ${result.error}`;
  }

  protected onIwaDevUpdateManifestUrlInput_(e: Event) {
    this.iwaDevUpdateManifestUrl_ = (e.target as HTMLInputElement).value;
  }

  protected onIwaDevUpdateManifestUrlKeyup_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      e.preventDefault();
      this.fetchUpdateManifest_();
    }
  }

  protected onIwaDevUpdateManifestFetchButtonClick_() {
    this.fetchUpdateManifest_();
  }

  private async fetchUpdateManifest_(): Promise<void> {
    const urlStr = this.iwaDevUpdateManifestUrl_;
    try {
      new URL(urlStr);
    } catch (_) {
      this.iwaDevInstallMessage_ =
          `Fetching the update manifest: ${urlStr} is not a valid URL`;
      return;
    }

    this.iwaDevInstallMessage_ = `Fetching the update manifest at ${urlStr}...`;

    const updateManifestUrl: Url = urlStr;
    const {result} =
        await this.browserProxy_.handler.parseUpdateManifestFromUrl(
            updateManifestUrl);
    if (result.error) {
      this.iwaDevInstallMessage_ = `Installing IWA from update manifest: ${
          urlStr} failed to install: ${result.error}`;
      return;
    }

    const manifest = result.updateManifest!;
    const versions = manifest.versions;

    versions.sort(
        (a: VersionEntry, b: VersionEntry) =>
            -compareStringVersions(a.version, b.version));

    this.selectedManifestVersions_ = versions;
    this.manifestVersions_ = versions;
    this.selectedManifestUrl_ = urlStr;

    this.$.updateManifestDialog.showModal();
  }

  protected onIwaUpdateManifestCloseClick_() {
    this.$.updateManifestDialog.close();
  }

  protected async onIwaUpdateManifestInstallClick_() {
    const select = this.$.updateManifestVersionSelect;
    const selectedVersion = select.value;
    this.$.updateManifestDialog.close();

    const updateManifestUrl = this.selectedManifestUrl_;
    const versions = this.selectedManifestVersions_;

    this.iwaDevInstallMessage_ =
        `Installing version ${selectedVersion} from ${updateManifestUrl}...`;
    const selectedVersionEntry: VersionEntry|undefined =
        versions.find(versionEntry => versionEntry.version === selectedVersion);

    if (!selectedVersionEntry) {
      this.iwaDevInstallMessage_ = `Installing version ${
          selectedVersion} from ${updateManifestUrl} failed: no such version`;
      return;
    }

    // TODO(crbug.com/373396075): Allow selecting the channel.
    const {result: installResult} =
        await this.browserProxy_.handler.installIsolatedWebAppFromBundleUrl({
          webBundleUrl: selectedVersionEntry.webBundleUrl,
          updateInfo: {
            updateManifestUrl,
            updateChannel: 'default',
            pinnedVersion: null,
            allowDowngrades: false,
          },
        });
    if (installResult.success) {
      this.iwaDevInstallMessage_ = `Installing version ${
          selectedVersion} from ${updateManifestUrl}: success!`;
    } else {
      this.iwaDevInstallMessage_ =
          `Installing version ${selectedVersion} from ${
              updateManifestUrl}: failed: ${installResult.error}`;
    }

    this.refreshDevModeAppList_();
  }

  protected onSwitchChannelClick_(e: Event) {
    const button = e.target as HTMLElement;
    this.activeAppId_ = button.dataset['appId'] || '';
    this.activeAppName_ = button.dataset['appName'] || '';
    this.$.switchChannelInputDialog.showModal();
  }

  protected onIwaSwitchChannelCloseClick_() {
    this.$.switchChannelInputDialog.close();
  }

  protected async onIwaSwitchChannelSwitchClick_() {
    const updateChannel = this.$.updateChannel;
    const appId = this.activeAppId_;
    const name = this.activeAppName_;
    this.$.switchChannelInputDialog.close();

    try {
      this.iwaDevInstallMessage_ =
          `Switching channel to ${updateChannel.value} for ${name}...`;

      const {success} =
          await this.browserProxy_.handler.setUpdateChannelForIsolatedWebApp(
              appId,
              updateChannel.value,
          );

      this.iwaDevInstallMessage_ = success ?
          `Successful channel switch to ${updateChannel.value} for ${name}.` :
          `Failed to switch channel to ${updateChannel.value} for ${name}.`;

      if (success) {
        this.refreshDevModeAppList_();
      }
    } catch (error) {
      this.iwaDevInstallMessage_ =
          `An error occurred while switching the update channel of ${name}.`;
      console.warn(error);
    }

    updateChannel.value = '';
  }

  protected onPinToVersionClick_(e: Event) {
    const button = e.target as HTMLElement;
    this.activeAppId_ = button.dataset['appId'] || '';
    this.activeAppName_ = button.dataset['appName'] || '';
    this.$.pinnedVersionInputDialog.showModal();
  }

  protected onIwaPinnedVersionCloseClick_() {
    this.$.pinnedVersionInputDialog.close();
    this.iwaDevInstallMessage_ = '';
  }

  protected onIwaPinnedVersionUnpinClick_() {
    this.$.pinnedVersionInputDialog.close();
    this.browserProxy_.handler.resetPinnedVersionForIsolatedWebApp(
        this.activeAppId_);
    this.refreshDevModeAppList_();
  }

  protected async onIwaPinnedVersionPinClick_() {
    const version = this.$.pinnedVersion.value;
    const appId = this.activeAppId_;
    const name = this.activeAppName_;
    this.iwaDevInstallMessage_ = `Pinning ${name} to version ${version}...`;
    this.$.pinnedVersionInputDialog.close();

    const {success} =
        await this.browserProxy_.handler.setPinnedVersionForIsolatedWebApp(
            appId, version);

    this.iwaDevInstallMessage_ = success ?
        `Successfully pinned ${name} to version ${
            version}; Version will be applied when an update is triggered.` :
        `Something went wrong while setting pinned version of ${
            name} to version ${version}.`;
    if (success) {
      await this.refreshDevModeAppList_();
    }
  }

  protected async onAllowDowngradesChange_(e: Event) {
    const input = e.target as HTMLInputElement;
    const appId = input.dataset['appId'];
    if (!appId) {
      return;
    }
    try {
      await this.browserProxy_.handler.setAllowDowngradesForIsolatedWebApp(
          input.checked, appId);
      await this.refreshDevModeAppList_();
    } catch (error) {
      this.iwaDevInstallMessage_ = 'Error toggling allowDowngrades';
    }
  }

  private getAppFromEvent_(e: Event): DevModeAppInfo|undefined {
    const button = e.target as HTMLElement;
    const appId = button.dataset['appId'];
    return this.devModeApps_.find(a => a.appId === appId);
  }

  protected async onUpdateAppClick_(e: Event) {
    const app = this.getAppFromEvent_(e);
    if (!app) {
      return;
    }
    app.isUpdating = true;
    app.updateMsg = '';
    this.requestUpdate();
    try {
      if (app.updateInfo) {
        const {result} = await this.browserProxy_.handler
                             .updateManifestInstalledIsolatedWebApp(app.appId);
        app.updateMsg = result;
      } else if (app.location.bundlePath) {
        const {result} =
            await this.browserProxy_.handler
                .selectFileAndUpdateIsolatedWebAppFromDevBundle(app.appId);
        app.updateMsg = result;
      } else if (app.location.proxyOrigin) {
        const {result} =
            await this.browserProxy_.handler.updateDevProxyIsolatedWebApp(
                app.appId);
        app.updateMsg = result;
      } else {
        assertNotReached();
      }
    } finally {
      app.isUpdating = false;
      this.requestUpdate();
    }
  }

  protected async onDeleteAppClick_(e: Event) {
    const app = this.getAppFromEvent_(e);
    if (!app) {
      return;
    }
    app.isDeleting = true;
    this.requestUpdate();
    try {
      const {success} =
          await this.browserProxy_.handler.deleteIsolatedWebApp(app.appId);

      if (success) {
        await this.refreshDevModeAppList_();
        this.iwaDevInstallMessage_ =
            `Successfully uninstalled ${app.name} (${app.webBundleId})`;
      } else {
        app.updateMsg = `Could not uninstall Isolated Web App "${app.name}" (${
            app.webBundleId})`;
        app.isDeleting = false;
        this.requestUpdate();
      }
    } catch (err) {
      app.updateMsg = `An error occurred during deletion of isolated Web App "${
          app.name}" (${app.webBundleId})`;
      app.isDeleting = false;
      this.requestUpdate();
      console.warn(err);
    }
  }

  protected getUpdateButtonLabel_(app: DevModeAppInfo): string {
    return app.isUpdating ?
        'Performing update... (close the IWA if it is currently open!)' :
        'Perform update now';
  }

  protected describeIsolatedWebApp_(app: IwaDevModeAppInfo): string {
    const updateMsg = `${app.name} (${app.installedVersion}) →`;
    if (app.updateInfo) {
      const pinnedVersionValue =
          app.updateInfo.pinnedVersion ? app.updateInfo.pinnedVersion : '-';
      return updateMsg +
          ` ${app.updateInfo.updateManifestUrl} ( update_channel: ${
                 app.updateInfo.updateChannel} | pinned_version: ${
                 pinnedVersionValue} | allow_downgrades: ${
                 app.updateInfo.allowDowngrades})`;
    }

    return updateMsg + ` (${formatDevModeLocation(app.location)})`;
  }

  private async refreshDevModeAppList_(): Promise<void> {
    this.devModeUpdatesMessage_ = 'Loading IWAs list...';

    const devModeApps: IwaDevModeAppInfo[] =
        (await this.browserProxy_.handler.getIsolatedWebAppDevModeAppInfo())
            .apps;

    if (devModeApps.length === 0) {
      this.devModeUpdatesMessage_ = 'None';
      this.devModeApps_ = [];
    } else {
      this.devModeUpdatesMessage_ = '';
      this.devModeApps_ = devModeApps;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'web-app-internals-app': WebAppInternalsAppElement;
  }
}

customElements.define(WebAppInternalsAppElement.is, WebAppInternalsAppElement);

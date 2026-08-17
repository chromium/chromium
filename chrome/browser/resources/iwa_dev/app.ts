// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './installed_app_list_item.js';
import './install_dialog.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Empty} from '//resources/mojo/mojo/public/mojom/base/empty.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {IwaDevInstallDialogElement} from './install_dialog.js';
import type {BrowserProxy, IwaDevModeAppInfo, UpdateInfo, UpdateManifest} from './iwa_dev.mojom-webui.js';
import {browserProxyFactory} from './iwa_dev.mojom-webui.js';

export const MIN_UPDATE_DELAY_MS = 750;

export interface IwaDevAppElement {
  $: {
    installButton: CrButtonElement,
    installDialog: IwaDevInstallDialogElement,
    toast: CrToastElement,
  };
}

export class IwaDevAppElement extends CrLitElement {
  static get is() {
    return 'iwa-dev-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      devModeEnabled_: {type: Boolean},
      installedApps_: {type: Array},
      hasFetchedApps_: {type: Boolean, state: true},
      updatingAppIds_: {type: Array, state: true},
      toastMessage_: {type: String},
    };
  }

  protected accessor devModeEnabled_: boolean =
      loadTimeData.getBoolean('isIwaDevModeEnabled');
  protected accessor installedApps_: IwaDevModeAppInfo[] = [];
  protected accessor hasFetchedApps_: boolean = false;
  protected accessor toastMessage_: string = '';
  protected accessor updatingAppIds_: string[] = [];
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private listenerIds_: number[] = [];

  protected async onRequestUpdate_(e: CustomEvent<{app: IwaDevModeAppInfo}>) {
    const app = e.detail.app;
    let updatePromise: Promise<unknown>;

    this.updatingAppIds_ = [...this.updatingAppIds_, app.appId];

    if (app.source.proxyOrigin) {
      updatePromise =
          this.browserProxy_.handler.updateDevProxyInstalledApp(app.appId);
    } else if (app.source.bundlePath) {
      updatePromise =
          this.browserProxy_.handler.selectAndUpdateAppFromLocalWebBundle(
              app.appId);
    } else if (app.source.updateInfo) {
      updatePromise =
          this.browserProxy_.handler.updateManifestInstalledApp(app.appId);
    } else {
      assertNotReached();
    }

    const timerPromise =
        new Promise(resolve => setTimeout(resolve, MIN_UPDATE_DELAY_MS));
    try {
      await updatePromise;
      this.toastMessage_ = 'Update successful!';
    } catch (err) {
      const errorMsg = (err as {message?: string})?.message || String(err);
      this.toastMessage_ = `Update failed: ${errorMsg}`;
    } finally {
      await timerPromise;
      this.$.toast.show();
      this.updatingAppIds_ =
          this.updatingAppIds_.filter(id => id !== app.appId);
    }
  }

  protected async onRequestUninstall_(
      e: CustomEvent<{app: IwaDevModeAppInfo}>) {
    await this.browserProxy_.handler.uninstallApp(e.detail.app.appId);
  }

  protected onOpenInstallDialogClick_() {
    this.$.installDialog.showDialog();
  }

  protected async onRequestInstallFromDevProxy_(e: CustomEvent<{url: Url}>) {
    await this.processInstallRequest_(
        this.browserProxy_.handler.installAppFromDevProxy(e.detail.url));
  }

  protected async onRequestInstallFromLocalBundle_() {
    await this.processInstallRequest_(
        this.browserProxy_.handler.selectAndInstallAppFromLocalWebBundle());
  }

  protected onRequestParseUpdateManifestFromUrl_(e: CustomEvent<{
    url: Url,
    callback: (result: {success?: UpdateManifest, error?: string}) => void,
  }>) {
    this.browserProxy_.handler.parseUpdateManifestFromUrl(e.detail.url)
        .then((success: UpdateManifest) => e.detail.callback({success}))
        .catch(
            err => e.detail.callback(
                {error: (err as {message?: string})?.message || String(err)}));
  }

  protected async onRequestInstallFromUpdateManifest_(e: CustomEvent<{
    webBundleUrl: Url,
    updateInfo: UpdateInfo,
  }>) {
    await this.processInstallRequest_(
        this.browserProxy_.handler.installAppFromUpdateManifest(
            e.detail.webBundleUrl, e.detail.updateInfo));
  }

  private processInstallRequest_(installPromise: Promise<Empty>) {
    const dialog = this.$.installDialog;
    dialog.startInstallation();

    return installPromise
        .then(() => {
          dialog.onInstallationFinished(null);
          this.toastMessage_ = 'Installation successful!';
          this.$.toast.show();
        })
        .catch(err => {
          const errorMessage =
              (err as {message?: string})?.message || String(err);
          dialog.onInstallationFinished(errorMessage);
        });
  }

  override async connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onAppInstalled.addListener(
            (appInfo: IwaDevModeAppInfo) => this.onAppInstalled_(appInfo)));
    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onAppUpdated.addListener(
            (appInfo: IwaDevModeAppInfo) => this.onAppUpdated_(appInfo)));
    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onAppUninstalled.addListener(
            (appId: string) => this.onAppUninstalled_(appId)));

    if (this.devModeEnabled_) {
      const {apps} = await this.browserProxy_.handler.getInstalledAppsInfo();
      this.installedApps_ = this.sortApps_(apps);
      this.hasFetchedApps_ = true;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  private sortApps_(apps: IwaDevModeAppInfo[]): IwaDevModeAppInfo[] {
    return apps.sort((a, b) => a.appId.localeCompare(b.appId));
  }

  private onAppInstalled_(appInfo: IwaDevModeAppInfo) {
    this.installedApps_ = this.sortApps_([...this.installedApps_, appInfo]);
  }

  private onAppUpdated_(appInfo: IwaDevModeAppInfo) {
    this.installedApps_ = this.installedApps_.map(
        app => app.appId === appInfo.appId ? appInfo : app);
  }

  private onAppUninstalled_(appId: string) {
    this.installedApps_ =
        this.installedApps_.filter(app => app.appId !== appId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-app': IwaDevAppElement;
  }
}

customElements.define(IwaDevAppElement.is, IwaDevAppElement);

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
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {IwaDevInstallDialogElement} from './install_dialog.js';
import type {BrowserProxy, IwaDevModeAppInfo} from './iwa_dev.mojom-webui.js';
import {browserProxyFactory} from './iwa_dev.mojom-webui.js';

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
      toastMessage_: {type: String},
    };
  }

  protected accessor devModeEnabled_: boolean =
      loadTimeData.getBoolean('isIwaDevModeEnabled');
  protected accessor installedApps_: IwaDevModeAppInfo[] = [];
  protected accessor hasFetchedApps_: boolean = false;
  protected accessor toastMessage_: string = '';
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private listenerIds_: number[] = [];

  protected async onRequestUninstall_(
      e: CustomEvent<{app: IwaDevModeAppInfo}>) {
    await this.browserProxy_.handler.uninstallApp(e.detail.app.appId);
  }

  protected onOpenInstallDialogClick_() {
    this.$.installDialog.showDialog();
  }

  protected async onRequestInstallFromDevProxy_(e: CustomEvent<{url: string}>) {
    await this.processInstallRequest_(
        this.browserProxy_.handler.installAppFromDevProxy(e.detail.url));
  }

  protected async onRequestInstallFromLocalBundle_() {
    await this.processInstallRequest_(
        this.browserProxy_.handler.selectAndInstallAppFromLocalWebBundle());
  }

  private async processInstallRequest_(
      installPromise: Promise<{error: string | null}>) {
    const dialog = this.$.installDialog;
    dialog.startInstallation();

    const {error} = await installPromise;

    dialog.onInstallationFinished(error);
    if (!error) {
      this.toastMessage_ = 'Installation successful!';
      this.$.toast.show();
    }
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

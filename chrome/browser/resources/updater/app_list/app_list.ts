// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../scope_icon.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {getKnownAppNamesById} from '../known_apps.js';
import type {AppState, GetAppStatesResponse} from '../updater_ui.mojom-webui.js';

import {getCss} from './app_list.css.js';
import {getHtml} from './app_list.html.js';

interface AppStateDisplay extends AppState {
  scope: 'SYSTEM'|'USER';
  displayName: string;
}

export class AppListElement extends CrLitElement {
  static get is() {
    return 'app-list';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      apps: {type: Array},
      error: {type: Boolean},
    };
  }

  protected accessor apps: AppStateDisplay[] = [];
  protected accessor error: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.getAppStates()
        .then(apps => this.apps = apps)
        .catch(() => this.error = true);
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async getAppStates(): Promise<AppStateDisplay[]> {
    const response: GetAppStatesResponse =
        await BrowserProxyImpl.getInstance().handler.getAppStates();
    const knownApps = getKnownAppNamesById();

    const systemApps: AppStateDisplay[] = response.systemApps.map(
        app => ({
          ...app,
          scope: 'SYSTEM',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    const userApps: AppStateDisplay[] = response.userApps.map(
        app => ({
          ...app,
          scope: 'USER',
          displayName: knownApps.get(app.appId.toLowerCase()) || app.appId,
        }));
    const apps = [...systemApps, ...userApps];
    return apps;
  }

  protected get shouldShowNoAppsMessage(): boolean {
    return !this.error && this.apps.length === 0;
  }

  protected get shouldShowTable(): boolean {
    return !this.error && this.apps.length !== 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }
}

customElements.define(AppListElement.is, AppListElement);

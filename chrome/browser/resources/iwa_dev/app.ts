// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './installed_app_list_item.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy, IwaDevModeAppInfo} from './iwa_dev.mojom-webui.js';
import {browserProxyFactory} from './iwa_dev.mojom-webui.js';

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
    };
  }

  protected accessor devModeEnabled_: boolean =
      loadTimeData.getBoolean('isIwaDevModeEnabled');
  protected accessor installedApps_: IwaDevModeAppInfo[] = [];
  protected accessor hasFetchedApps_: boolean = false;
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  override async connectedCallback() {
    super.connectedCallback();
    if (this.devModeEnabled_) {
      const {apps} = await this.browserProxy_.handler.getInstalledAppsInfo();
      this.installedApps_ = apps;
      this.hasFetchedApps_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-app': IwaDevAppElement;
  }
}

customElements.define(IwaDevAppElement.is, IwaDevAppElement);

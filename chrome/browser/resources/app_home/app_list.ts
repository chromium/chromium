// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AppInfo} from './app_home.mojom-webui.js';

import {getTemplate} from './app_list.html.js';
import {BrowserProxy} from './browser_proxy.js';


export class AppListElement extends PolymerElement {
  static get is() {
    return 'app-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apps_: Array,
    };
  }

  private apps_: AppInfo[];

  // get applist data to main page
  override ready() {
    super.ready();
    const instance = BrowserProxy.getInstance().handler;
    instance.getApps().then((result) => {
      this.apps_ = result.appList;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }
}

customElements.define(AppListElement.is, AppListElement);

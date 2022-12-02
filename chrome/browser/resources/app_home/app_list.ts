// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppInfo, PageCallbackRouter} from './app_home.mojom-webui.js';
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
      apps_: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  private apps_: AppInfo[];
  private mojoEventTarget_: PageCallbackRouter;
  private listenerIds_: number[];

  constructor() {
    super();

    this.mojoEventTarget_ = BrowserProxy.getInstance().callbackRouter;

    BrowserProxy.getInstance().handler.getApps().then(result => {
      this.apps_ = result.appList;
    });
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.mojoEventTarget_.addApp.addListener(this.addApp_.bind(this)),
      this.mojoEventTarget_.removeApp.addListener(this.removeApp_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_.removeListener(id)));
    this.listenerIds_ = [];
  }

  private addApp_(data: AppInfo) {
    this.push('apps_', data);
  }

  private removeApp_(data: AppInfo) {
    const index = this.apps_.findIndex(app => app.id === data.id);
    // We gracefully handle item not found case because:
    // 1.if the async getApps() returns later than an uninstall event,
    // it should gracefully handles that and ignores that uninstall event,
    // the getApps() will return the list without the app later.
    // 2.If an uninstall event gets fired for an app that's somehow not in
    // the list of apps shown in current page, it's none of the concern
    // for this page to remove it.
    if (index !== -1) {
      this.splice('apps_', index, 1);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-list': AppListElement;
  }
}

customElements.define(AppListElement.is, AppListElement);

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_service_internals.html.js';
import type {AppCapabilityInfo, AppInfo, PreferredAppInfo, PromiseAppInfo} from './app_service_internals.mojom-webui.js';
import {AppServiceInternalsPageHandler} from './app_service_internals.mojom-webui.js';

export class AppServiceInternalsElement extends PolymerElement {
  static get is() {
    return 'app-service-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appList_: Array,
      preferredAppList_: Array,
      promiseAppList_: Array,
      appCapabilityList_: Array,
    };
  }

  /** List containing debug information for all installed apps. */
  private appList_: AppInfo[] = [];
  private hashChangeListener_ = () => this.onHashChanged_();
  /** List containing preferred app debug information for installed apps. */
  private preferredAppList_: PreferredAppInfo[] = [];
  /** List containing debug information for all promise apps. */
  private promiseAppList_: PromiseAppInfo[] = [];
  /** List containing app capability access information. */
  private appCapabilityList_: AppCapabilityInfo[] = [];

  override ready() {
    super.ready();
    (async () => {
      const remote = AppServiceInternalsPageHandler.getRemote();

      const {debugInfo} = await remote.getDebugInfo();
      if (debugInfo) {
        this.appList_ = debugInfo.appList;
        this.preferredAppList_ = debugInfo.preferredAppList;
        this.promiseAppList_ = debugInfo.promiseAppList;
        this.appCapabilityList_ = debugInfo.appCapabilityList;
      }
      window.addEventListener('hashchange', this.hashChangeListener_);
      // setTimeout ensures that we only apply the hash change after all the
      // page content has rendered.
      setTimeout(() => this.onHashChanged_(), 0);
    })();
  }

  override disconnectedCallback() {
    window.removeEventListener('hashchange', this.hashChangeListener_);
  }

  /**
   * Manually responds to URL hash changes, since the regular browser handling
   * doesn't work in the Shadow DOM.
   */
  private onHashChanged_() {
    if (!location.hash || !this.shadowRoot) {
      window.scrollTo(0, 0);
      return;
    }

    const selected = this.shadowRoot.querySelector(location.hash);
    if (!selected) {
      return;
    }

    selected.scrollIntoView();
  }

  private save_() {
    const fileParts: string[] = [];
    fileParts.push('App List\n');
    fileParts.push('========\n\n');
    for (const app of this.appList_) {
      fileParts.push(app.name + '\n');
      fileParts.push('-----\n');
      fileParts.push(app.debugInfo + '\n');
    }

    fileParts.push('Preferred Apps\n');
    fileParts.push('==============\n\n');
    for (const preferredApp of this.preferredAppList_) {
      fileParts.push(preferredApp.name + ' (' + preferredApp.id + ')\n');
      fileParts.push('-----\n');
      fileParts.push(preferredApp.preferredFilters + '\n');
    }

    fileParts.push('App Capabilities\n');
    fileParts.push('================\n\n');
    for (const appCapability of this.appCapabilityList_) {
      fileParts.push(appCapability.name + '\n');
      fileParts.push('-----\n');
      fileParts.push(appCapability.debugInfo + '\n');
    }

    fileParts.push('Promise App List\n');
    fileParts.push('================\n\n');
    for (const promiseApp of this.promiseAppList_) {
      fileParts.push(promiseApp.packageId + '\n');
      fileParts.push('-----\n');
      fileParts.push(promiseApp.debugInfo + '\n');
    }

    const file = new Blob(fileParts);
    const a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = 'app-service-internals.txt';
    a.click();
    URL.revokeObjectURL(a.href);
  }
}

customElements.define(
    AppServiceInternalsElement.is, AppServiceInternalsElement);

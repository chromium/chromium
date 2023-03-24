// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app_service_internals.html.js';
import {AppInfo, AppServiceInternalsPageHandler, PreferredAppInfo, PromiseAppInfo} from './app_service_internals.mojom-webui.js';

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
    };
  }

  /** List containing debug information for all installed apps. */
  private appList_: AppInfo[] = [];
  private hashChangeListener_ = () => this.onHashChanged_();
  /** List containing preferred app debug information for installed apps. */
  private preferredAppList_: PreferredAppInfo[] = [];
  /** List containing debug information for all promise apps. */
  private promiseAppList_: PromiseAppInfo[] = [];

  override ready() {
    super.ready();
    (async () => {
      const remote = AppServiceInternalsPageHandler.getRemote();

      this.appList_ = (await remote.getApps()).appList;
      this.preferredAppList_ =
          (await remote.getPreferredApps()).preferredAppList;
      this.promiseAppList_ = (await remote.getPromiseApps()).promiseAppList;
      this.onHashChanged_();
      window.addEventListener('hashchange', this.hashChangeListener_);
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
    const fileParts = [];
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
      fileParts.push(preferredApp.name + '\n');
      fileParts.push('-----\n');
      fileParts.push(preferredApp.preferredFilters + '\n');
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

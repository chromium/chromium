// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppInfo, AppServiceInternalsPageHandler} from './app_service_internals.mojom-webui.js';

export class AppServiceInternalsElement extends PolymerElement {
  static get is() {
    return 'app-service-internals';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      appList_: Array,
    };
  }

  /** List containing debug information for all installed apps. */
  appList_: Array<AppInfo> = [];

  ready() {
    super.ready();
    (async () => {
      const remote = AppServiceInternalsPageHandler.getRemote();

      this.appList_ = (await remote.getApps()).appList;
      this.appList_.sort((a, b) => a.name.localeCompare(b.name));
    })();
  }
}

customElements.define(
    AppServiceInternalsElement.is, AppServiceInternalsElement);

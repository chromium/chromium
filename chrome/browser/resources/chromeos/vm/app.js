// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticEntry_Status as Status} from './guest_os_diagnostics.mojom-webui.js';
import {VmDiagnosticsProvider} from './vm.mojom-webui.js';

class VmApp extends PolymerElement {
  static get properties() {
    return {
      title_: {type: String},
      // If the url is recognized, we show the diagnostics. Otherwise, we show
      // the contents page.
      showContentsPage_: {type: Boolean},
      diagnostics_: {type: Object},
    };
  }

  static get template() {
    return html`{__html_template__}`;
  }

  ready() {
    super.ready();

    this.init();
  }

  async init() {
    const url = new URL(window.location.href);
    switch (url.pathname) {
      case '/parallels':
        this.setTitle_(this.getTitle('pluginVmAppName'));
        this.showContentsPage_ = false;
        this.diagnostics_ =
            (await VmDiagnosticsProvider.getRemote().getPluginVmDiagnostics())
                .diagnostics;
        break;
      default:
        // Show the contents page if the url is unknown.
        this.setTitle_(loadTimeData.getString('contentsPageTitle'));
        this.showContentsPage_ = true;
        break;
    }
  }

  /** @private */
  setTitle_(title) {
    this.title_ = title;
    document.title = this.title_;
  }

  statusToString(statusValue) {
    let stringId = '';
    switch (statusValue) {
      case Status.kPass:
        stringId = 'passLabel';
        break;
      case Status.kFail:
        stringId = 'failLabel';
        break;
      case Status.kNotApplicable:
        stringId = 'notApplicableLabel';
        break;
    }
    return loadTimeData.getString(stringId);
  }

  statusToClass(statusValue) {
    switch (statusValue) {
      case Status.kPass:
        return 'pass';
      case Status.kFail:
        return 'fail';
      case Status.kNotApplicable:
        return '';
    }
  }

  getTitle(appNameId) {
    return loadTimeData.getStringF(
        'pageTitle', loadTimeData.getString(appNameId));
  }

  formatTopErrorMessage(topErrorMessage) {
    return loadTimeData.getStringF('notEnabledMessage', topErrorMessage);
  }
}

customElements.define('vm-app', VmApp);

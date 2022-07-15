// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared.css.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './data_export_done.html.js';
import {SupportToolPageMixin} from './support_tool_page_mixin.js';

const DataExportDoneElementBase = SupportToolPageMixin(PolymerElement);

export class DataExportDoneElement extends DataExportDoneElementBase {
  static get is() {
    return 'data-export-done';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path_: {
        type: String,
        value: '',
      },
    };
  }

  private path_: string;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private onFilePathClicked_() {
    this.browserProxy_.showExportedDataInFolder();
  }

  setPath(path: string) {
    this.path_ = path;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-export-done': DataExportDoneElement;
  }
}

customElements.define(DataExportDoneElement.is, DataExportDoneElement);

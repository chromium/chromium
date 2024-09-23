// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
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

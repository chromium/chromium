// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './data_export_done.css.js';
import {getHtml} from './data_export_done.html.js';
import {SupportToolPageMixinLit} from './support_tool_page_mixin_lit.js';

const DataExportDoneElementBase = SupportToolPageMixinLit(CrLitElement);

export class DataExportDoneElement extends DataExportDoneElementBase {
  static get is() {
    return 'data-export-done';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      path_: {type: String},
    };
  }

  protected accessor path_: string = '';
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected onFilePathClick_() {
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

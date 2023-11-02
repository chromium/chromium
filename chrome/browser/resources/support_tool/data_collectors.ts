// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl, DataCollectorItem} from './browser_proxy.js';
import {getTemplate} from './data_collectors.html.js';
import {SupportToolPageMixin} from './support_tool_page_mixin.js';

const DataCollectorsElementBase = SupportToolPageMixin(PolymerElement);

export class DataCollectorsElement extends DataCollectorsElementBase {
  static get is() {
    return 'data-collectors';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dataCollectors_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private dataCollectors_: DataCollectorItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
        });
  }

  getDataCollectors(): DataCollectorItem[] {
    return this.dataCollectors_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-collectors': DataCollectorsElement;
  }
}

customElements.define(DataCollectorsElement.is, DataCollectorsElement);

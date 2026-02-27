// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, DataCollectorItem} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './data_collectors.css.js';
import {getHtml} from './data_collectors.html.js';
import {SupportToolPageMixinLit} from './support_tool_page_mixin_lit.js';

const DataCollectorsElementBase = SupportToolPageMixinLit(CrLitElement);

export class DataCollectorsElement extends DataCollectorsElementBase {
  static get is() {
    return 'data-collectors';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataCollectors_: {type: Array},
      allSelected_: {type: Boolean},
    };
  }

  protected accessor dataCollectors_: DataCollectorItem[] = [];
  protected accessor allSelected_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
          this.allSelected_ =
              this.dataCollectors_.every((element) => element.isIncluded);
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('allSelected_')) {
      // Update this.dataCollectors_ to reflect the selection choice.
      this.dataCollectors_ = this.dataCollectors_.map(
          item => ({...item, isIncluded: this.allSelected_}));
    }
  }

  getDataCollectors(): DataCollectorItem[] {
    return this.dataCollectors_;
  }

  protected onAllSelectedCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.allSelected_ = e.detail.value;
  }

  protected onDataCollectorCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    const isIncluded = e.detail.value;
    this.dataCollectors_[index]!.isIncluded = isIncluded;
    this.requestUpdate();  // Trigger Lit update
    this.allSelected_ = this.dataCollectors_.every(item => item.isIncluded);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-collectors': DataCollectorsElement;
  }
}

customElements.define(DataCollectorsElement.is, DataCollectorsElement);

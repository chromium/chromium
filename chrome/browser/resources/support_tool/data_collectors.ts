// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './screenshot.js';
import './support_tool_shared.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy, DataCollectorItem} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './data_collectors.html.js';
import type {ScreenshotElement} from './screenshot.js';
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
      enableScreenshot_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableScreenshot'),
      },
      allSelected_: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onAllSelectedChanged_',
      },
    };
  }

  private dataCollectors_: DataCollectorItem[];
  private enableScreenshot_: boolean;
  private allSelected_: boolean;
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

  getDataCollectors(): DataCollectorItem[] {
    return this.dataCollectors_;
  }

  setScreenshotData(dataBase64: string) {
    if (this.enableScreenshot_) {
      this.$$<ScreenshotElement>('#screenshot')!.setScreenshotData(dataBase64);
    }
  }

  getEditedScreenshotBase64(): string {
    // `SupportToolMessageHandler` will handle the case when the screenshot
    // feature is disabled.
    return this.enableScreenshot_ ?
        this.$$<ScreenshotElement>('#screenshot')!.getEditedScreenshotBase64() :
        '';
  }

  private onAllSelectedChanged_() {
    // Update this.dataCollectors_ to reflect the selection choice.
    for (let index = 0; index < this.dataCollectors_.length; index++) {
      // Mutate the array observably. See:
      // https://polymer-library.polymer-project.org/3.0/docs/devguide/data-system#make-observable-changes
      this.set(`dataCollectors_.${index}.isIncluded`, this.allSelected_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-collectors': DataCollectorsElement;
  }
}

customElements.define(DataCollectorsElement.is, DataCollectorsElement);

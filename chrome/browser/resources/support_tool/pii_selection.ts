// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './support_tool_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, PIIDataItem} from './browser_proxy.js';
import {getTemplate} from './pii_selection.html.js';

export class PIISelectionElement extends PolymerElement {
  static get is() {
    return 'pii-selection';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectAll_: {
        type: Boolean,
        value: false,
        observer: 'onSelectAllChanged_',
      },
      detectedPIIItems_: {
        type: Array,
        value: () => [],
      }
    };
  }

  private selectAll_: boolean;
  private detectedPIIItems_: PIIDataItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  updateDetectedPIIItems(items: PIIDataItem[]) {
    items.forEach((item) => {
      item.expandDetails = true;
    });
    this.detectedPIIItems_ = items;
  }

  private onCancelClick_() {
    this.browserProxy_.cancelDataCollection();
  }

  private onExportClick_() {
    this.browserProxy_.startDataExport(this.detectedPIIItems_);
  }

  private onSelectAllChanged_() {
    // Update this.detectedPIIItems_ to reflect the selection choice.
    for (let index = 0; index < this.detectedPIIItems_.length; index++) {
      // Mutate the array observably. See:
      // https://polymer-library.polymer-project.org/3.0/docs/devguide/data-system#make-observable-changes
      this.set(`detectedPIIItems_.${index}.keep`, this.selectAll_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pii-selection': PIISelectionElement;
  }
}

customElements.define(PIISelectionElement.is, PIISelectionElement);
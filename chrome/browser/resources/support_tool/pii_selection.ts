// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './support_tool_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl, PIIDataItem} from './browser_proxy.js';
import {getTemplate} from './pii_selection.html.js';

// Names of the radio buttons which allow the user to choose to keep or remove
// their PII data.
enum PiiRadioButtons {
  INCLUDE_ALL = 'include-all',
  INCLUDE_NONE = 'include-none',
  INCLUDE_SOME = 'include-some',
}

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
      },
      detectedPIIItems_: {
        type: Array,
        value: () => [],
      },
      piiRadioButtonsEnum_: {
        readonly: true,
        type: Object,
        value: PiiRadioButtons,
      },
      selectedRadioButton_: {
        type: String,
        value: PiiRadioButtons.INCLUDE_NONE,
      },
      showPIISelection_: {
        type: Boolean,
        value: false,
      }
    };
  }

  private selectAll_: boolean;
  private selectedRadioButton_: string;
  private showPIISelection_: boolean;
  private detectedPIIItems_: PIIDataItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  updateDetectedPIIItems(items: PIIDataItem[]) {
    items.forEach((item) => {
      item.expandDetails = false;
    });
    this.detectedPIIItems_ = items;
  }

  private onCancelClick_() {
    this.browserProxy_.cancelDataCollection();
  }

  private onExportClick_() {
    this.browserProxy_.startDataExport(this.detectedPIIItems_);
  }

  // Sets this.selectAll_ and updates this.detectedPIIItems_ contents
  // accordingly.
  private setSelectAll_(selectAll: boolean) {
    this.selectAll_ = selectAll;
    // We won't be showing PII selection checkboxes when this.selectAll_ is set.
    this.showPIISelection_ = false;
    // Update this.detectedPIIItems_ to reflect the selection choice.
    for (let index = 0; index < this.detectedPIIItems_.length; index++) {
      // Mutate the array observably. See:
      // https://polymer-library.polymer-project.org/3.0/docs/devguide/data-system#make-observable-changes
      this.set(`detectedPIIItems_.${index}.keep`, this.selectAll_);
    }
  }

  private onSelectedRadioButtonChanged_(event: CustomEvent<{value: string}>) {
    this.selectedRadioButton_ = event.detail.value;
    if (this.selectedRadioButton_ === PiiRadioButtons.INCLUDE_ALL) {
      this.setSelectAll_(true);
    } else if (this.selectedRadioButton_ === PiiRadioButtons.INCLUDE_NONE) {
      this.setSelectAll_(false);
    } else {
      this.showPIISelection_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pii-selection': PIISelectionElement;
  }
}

customElements.define(PIISelectionElement.is, PIISelectionElement);

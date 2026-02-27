// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, PiiDataItem} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './pii_selection.css.js';
import {getHtml} from './pii_selection.html.js';
import {SupportToolPageMixinLit} from './support_tool_page_mixin_lit.js';

// Names of the radio buttons which allow the user to choose to keep or remove
// their PII data.
export enum PiiRadioButtons {
  INCLUDE_ALL = 'include-all',
  INCLUDE_NONE = 'include-none',
  INCLUDE_SOME = 'include-some',
}

const PiiSelectionElementBase = SupportToolPageMixinLit(CrLitElement);

export class PiiSelectionElement extends PiiSelectionElementBase {
  static get is() {
    return 'pii-selection';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectAll_: {type: Boolean},
      detectedPiiItems_: {type: Array},
      selectedRadioButton_: {type: String},
      showPIISelection_: {type: Boolean},
    };
  }

  protected accessor selectAll_: boolean = true;
  protected accessor detectedPiiItems_: PiiDataItem[] = [];
  protected accessor selectedRadioButton_: PiiRadioButtons =
      PiiRadioButtons.INCLUDE_ALL;
  protected accessor showPIISelection_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  updateDetectedPiiItems(items: PiiDataItem[]) {
    items.forEach((item) => {
      item.expandDetails = false;
    });
    this.detectedPiiItems_ = [...items];
  }

  protected onCancelClick_() {
    this.browserProxy_.cancelDataCollection();
  }

  protected onExportClick_() {
    this.browserProxy_.startDataExport(this.detectedPiiItems_);
  }

  // Sets this.selectAll_ and updates this.detectedPiiItems_ contents
  // accordingly.
  private setSelectAll_(selectAll: boolean) {
    this.selectAll_ = selectAll;
    // We won't be showing PII selection checkboxes when this.selectAll_ is set.
    this.showPIISelection_ = false;
    // Update this.detectedPiiItems_ to reflect the selection choice.
    this.detectedPiiItems_ =
        this.detectedPiiItems_.map(item => ({...item, keep: this.selectAll_}));
  }

  protected onSelectedRadioButtonSelectedChanged_(
      event: CustomEvent<{value: string}>) {
    this.selectedRadioButton_ = event.detail.value as PiiRadioButtons;
    switch (this.selectedRadioButton_) {
      case PiiRadioButtons.INCLUDE_ALL:
        this.setSelectAll_(true);
        break;
      case PiiRadioButtons.INCLUDE_NONE:
        this.setSelectAll_(false);
        break;
      case PiiRadioButtons.INCLUDE_SOME:
        this.showPIISelection_ = true;
        break;
      default:
        assertNotReachedCase(this.selectedRadioButton_);
    }
  }

  protected showDisclaimer_(): boolean {
    return this.selectedRadioButton_ === PiiRadioButtons.INCLUDE_NONE;
  }

  protected getPiiItemAriaLabel_(item: PiiDataItem): string {
    return 'More info for ' + item.piiTypeDescription + ' ' + item.count;
  }

  protected onPiiExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    this.detectedPiiItems_[index]!.expandDetails = e.detail.value;
    this.requestUpdate();
  }

  protected onPiiCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    this.detectedPiiItems_[index]!.keep = e.detail.value;
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pii-selection': PiiSelectionElement;
  }
}

customElements.define(PiiSelectionElement.is, PiiSelectionElement);

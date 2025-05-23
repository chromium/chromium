// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/util.js';
import './icons.html.js';
import '/strings.m.js';

import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';
import {PDF_DESTINATION_KEY} from '../data/destination.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {getCss} from './destination_select.css.js';
import {getHtml} from './destination_select.html.js';
import {SelectMixin} from './select_mixin.js';

const PrintPreviewDestinationSelectElementBase = SelectMixin(CrLitElement);

export class PrintPreviewDestinationSelectElement extends
    PrintPreviewDestinationSelectElementBase {
  static get is() {
    return 'print-preview-destination-select';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dark: {type: Boolean},
      destination: {type: Object},
      disabled: {type: Boolean},
      loaded: {type: Boolean},
      noDestinations: {type: Boolean},
      pdfPrinterDisabled: {type: Boolean},
      recentDestinationList: {type: Array},
      pdfDestinationKey_: {type: String},
    };
  }

  accessor dark: boolean = false;
  accessor destination: Destination|null = null;
  accessor disabled: boolean = false;
  accessor loaded: boolean = false;
  accessor noDestinations: boolean = false;
  accessor pdfPrinterDisabled: boolean = false;
  accessor recentDestinationList: Destination[] = [];
  protected accessor pdfDestinationKey_: string = PDF_DESTINATION_KEY;

  override focus() {
    this.shadowRoot.querySelector<HTMLElement>('.md-select')!.focus();
  }

  /** Sets the select to the current value of |destination|. */
  updateDestination() {
    this.selectedValue = this.destination?.key || '';
  }

  /**
   * Returns the iconset and icon for the selected printer. If printer details
   * have not yet been retrieved from the backend, attempts to return an
   * appropriate icon early based on the printer's sticky information.
   * @return The iconset and icon for the current selection.
   */
  private getDestinationIcon_(): string {
    if (!this.selectedValue) {
      return '';
    }

    // If the destination matches the selected value, pull the icon from the
    // destination.
    if (this.destination && this.destination.key === this.selectedValue) {
      return this.destination.icon;
    }

    // Check for the Save as PDF id first.
    if (this.selectedValue === PDF_DESTINATION_KEY) {
      return 'cr:insert-drive-file';
    }

    // Otherwise, must be in the recent list.
    const recent = this.recentDestinationList.find(d => {
      return d.key === this.selectedValue;
    });
    if (recent && recent.icon) {
      return recent.icon;
    }

    // The key/recent destinations don't have information about what icon to
    // use, so just return the generic print icon for now. It will be updated
    // when the destination is set.
    return 'print-preview:print';
  }

  /**
   * @return An inline svg corresponding to the icon for the current
   *     destination and the image for the dropdown arrow.
   */
  protected getBackgroundImages_(): string {
    const icon = this.getDestinationIcon_();
    if (!icon) {
      return '';
    }

    let iconSetAndIcon = null;
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    iconSetAndIcon = iconSetAndIcon || icon.split(':');

    const iconset = IconsetMap.getInstance().get(iconSetAndIcon[0]!);
    assert(iconset);
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1]!, this);
  }

  override onProcessSelectChange(value: string) {
    this.fire('selected-option-change', value);
  }

  /**
   * Return the options currently visible to the user for testing purposes.
   */
  getVisibleItemsForTest(): NodeListOf<HTMLOptionElement> {
    return this.shadowRoot.querySelectorAll<HTMLOptionElement>(
        'option:not([hidden])');
  }

  protected isSelected_(destinationKey: string): boolean {
    return this.selectedValue === destinationKey;
  }
}

export type DestinationSelectElement = PrintPreviewDestinationSelectElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-select': PrintPreviewDestinationSelectElement;
  }
}

customElements.define(
    PrintPreviewDestinationSelectElement.is,
    PrintPreviewDestinationSelectElement);

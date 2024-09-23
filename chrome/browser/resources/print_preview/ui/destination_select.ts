// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Note: Chrome OS uses print-preview-destination-select-cros rather than the
 * element in this file. Ensure any fixes for cross platform bugs work on both
 * Chrome OS and non-Chrome OS.
 */

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/js/util.js';
import './destination_select_style.css.js';
import './icons.html.js';
import './print_preview_shared.css.js';
import './throbber.css.js';
import '../strings.m.js';

import {IconsetMap} from 'chrome://resources/cr_elements/cr_icon/iconset_map.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {PDF_DESTINATION_KEY} from '../data/destination.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {getTemplate} from './destination_select.html.js';
import {SelectMixin} from './select_mixin.js';

const PrintPreviewDestinationSelectElementBase =
    I18nMixin(SelectMixin(PolymerElement));

export class PrintPreviewDestinationSelectElement extends
    PrintPreviewDestinationSelectElementBase {
  static get is() {
    return 'print-preview-destination-select';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeUser: String,

      dark: Boolean,

      destination: Object,

      disabled: Boolean,

      loaded: Boolean,

      noDestinations: Boolean,

      pdfPrinterDisabled: Boolean,

      recentDestinationList: Array,

      pdfDestinationKey_: {
        type: String,
        value: PDF_DESTINATION_KEY,
      },
    };
  }

  activeUser: string;
  dark: boolean;
  destination: Destination;
  disabled: boolean;
  loaded: boolean;
  noDestinations: boolean;
  pdfPrinterDisabled: boolean;
  recentDestinationList: Destination[];
  private pdfDestinationKey_: string;

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('.md-select')!.focus();
  }

  /** Sets the select to the current value of |destination|. */
  updateDestination() {
    this.selectedValue = this.destination.key;
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
  private getBackgroundImages_(): string {
    const icon = this.getDestinationIcon_();
    if (!icon) {
      return '';
    }

    let iconSetAndIcon = null;
    if (this.noDestinations) {
      iconSetAndIcon = ['cr', 'error'];
    }
    iconSetAndIcon = iconSetAndIcon || icon.split(':');

    const iconset = IconsetMap.getInstance().get(iconSetAndIcon[0]);
    assert(iconset);
    return getSelectDropdownBackground(iconset, iconSetAndIcon[1], this);
  }

  override onProcessSelectChange(value: string) {
    this.dispatchEvent(new CustomEvent(
        'selected-option-change',
        {bubbles: true, composed: true, detail: value}));
  }

  /**
   * Return the options currently visible to the user for testing purposes.
   */
  getVisibleItemsForTest(): NodeListOf<HTMLOptionElement> {
    return this.shadowRoot!.querySelectorAll<HTMLOptionElement>(
        'option:not([hidden])');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-select': PrintPreviewDestinationSelectElement;
  }
}

customElements.define(
    PrintPreviewDestinationSelectElement.is,
    PrintPreviewDestinationSelectElement);

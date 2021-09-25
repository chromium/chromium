// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// TODO(gavinwill): Remove iron-dropdown dependency https://crbug.com/1082587.
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import './print_preview_vars_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, PrinterStatusReason} from '../data/printer_status_cros.js';


declare global {
  interface HTMLElementEventMap {
    'dropdown-value-selected': CustomEvent<HTMLButtonElement>;
  }
}

const PrintPreviewDestinationDropdownCrosElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

export class PrintPreviewDestinationDropdownCrosElement extends
    PrintPreviewDestinationDropdownCrosElementBase {
  static get is() {
    return 'print-preview-destination-dropdown-cros';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      value: Object,

      itemList: {
        type: Array,
        observer: 'enqueueDropdownRefit_',
      },

      disabled: {
        type: Boolean,
        value: false,
        observer: 'updateTabIndex_',
        reflectToAttribute: true,
      },

      driveDestinationKey: String,

      noDestinations: Boolean,

      pdfPrinterDisabled: Boolean,

      pdfDestinationKey: String,

      destinationIcon: String,

      /**
       * Index of the highlighted item in the dropdown.
       */
      highlightedIndex_: Number,

      dropdownLength_: {
        type: Number,
        computed: 'computeDropdownLength_(itemList, pdfPrinterDisabled, ' +
            'driveDestinationKey, noDestinations)',
      },

      destinationStatusText: String,
    };
  }

  value: Destination;
  itemList: Destination[];
  disabled: boolean;
  driveDestinationKey: string;
  noDestinations: boolean;
  pdfPrinterDisabled: boolean;
  destinationStatusText: string;
  private highlightedIndex_: number;
  private dropdownLength_: number;

  private opened_: boolean = false;
  private dropdownRefitPending_: boolean = false;

  ready() {
    super.ready();

    this.addEventListener('mousemove', e => this.onMouseMove_(e));
  }

  connectedCallback() {
    super.connectedCallback();

    this.updateTabIndex_();
  }

  focus() {
    this.shadowRoot!.querySelector<HTMLElement>(
                        '#destination-dropdown')!.focus();
  }

  private fireDropdownValueSelected_(element: Element) {
    this.dispatchEvent(new CustomEvent(
        'dropdown-value-selected',
        {bubbles: true, composed: true, detail: element}));
  }

  /**
   * Enqueues a task to refit the iron-dropdown if it is open.
   */
  private enqueueDropdownRefit_() {
    const dropdown = this.shadowRoot!.querySelector('iron-dropdown')!;
    if (!this.dropdownRefitPending_ && dropdown.opened) {
      this.dropdownRefitPending_ = true;
      setTimeout(() => {
        dropdown.refit();
        this.dropdownRefitPending_ = false;
      }, 0);
    }
  }

  private openDropdown_() {
    if (this.disabled) {
      return;
    }

    this.highlightedIndex_ = this.getButtonListFromDropdown_().findIndex(
        item => item.value === this.value.key);
    this.shadowRoot!.querySelector('iron-dropdown')!.open();
    this.opened_ = true;
  }

  private closeDropdown_() {
    this.shadowRoot!.querySelector('iron-dropdown')!.close();
    this.opened_ = false;
    this.highlightedIndex_ = -1;
  }

  /**
   * Highlight the item the mouse is hovering over. If the user uses the
   * keyboard, the highlight will shift. But once the user moves the mouse,
   * the highlight should be updated based on the location of the mouse
   * cursor.
   * @param {!Event} event
   * @private
   */
  private onMouseMove_(event: Event) {
    const item =
        (event.composedPath() as HTMLElement[])
            .find(elm => elm.classList && elm.classList.contains('list-item'));
    if (!item) {
      return;
    }
    this.highlightedIndex_ =
        this.getButtonListFromDropdown_().indexOf(item as HTMLButtonElement);
  }

  private onClick_(event: Event) {
    const dropdown = this.shadowRoot!.querySelector('iron-dropdown')!;
    // Exit if path includes |dropdown| because event will be handled by
    // onSelect_.
    if (event.composedPath().includes(dropdown)) {
      return;
    }

    if (dropdown.opened) {
      this.closeDropdown_();
      return;
    }
    this.openDropdown_();
  }

  private onSelect_(event: Event) {
    this.dropdownValueSelected_(event.currentTarget as Element);
  }

  private onKeyDown_(event: KeyboardEvent) {
    event.stopPropagation();
    const dropdown = this.shadowRoot!.querySelector('iron-dropdown')!;
    switch (event.code) {
      case 'ArrowUp':
      case 'ArrowDown':
        this.onArrowKeyPress_(event.code);
        break;
      case 'Enter': {
        if (dropdown.opened) {
          this.dropdownValueSelected_(
              this.getButtonListFromDropdown_()[this.highlightedIndex_]);
          break;
        }
        this.openDropdown_();
        break;
      }
      case 'Escape': {
        if (dropdown.opened) {
          this.closeDropdown_();
          event.preventDefault();
        }
        break;
      }
    }
  }

  private onArrowKeyPress_(eventCode: string) {
    const dropdown = this.shadowRoot!.querySelector('iron-dropdown')!;
    const items = this.getButtonListFromDropdown_();
    if (items.length === 0) {
      return;
    }

    // If the dropdown is open, use the arrow key press to change which item is
    // highlighted in the dropdown. If the dropdown is closed, use the arrow key
    // press to change the selected destination.
    if (dropdown.opened) {
      const nextIndex = this.getNextItemIndexInList_(
          eventCode, this.highlightedIndex_, items.length);
      if (nextIndex === -1) {
        return;
      }
      this.highlightedIndex_ = nextIndex;
      items[this.highlightedIndex_].focus();
      return;
    }

    const currentIndex = items.findIndex(item => item.value === this.value.key);
    const nextIndex =
        this.getNextItemIndexInList_(eventCode, currentIndex, items.length);
    if (nextIndex === -1) {
      return;
    }
    this.fireDropdownValueSelected_(items[nextIndex]);
  }

  /**
   * @return -1 when the next item would be outside the list.
   */
  private getNextItemIndexInList_(
      eventCode: string, currentIndex: number, numItems: number): number {
    const nextIndex =
        eventCode === 'ArrowDown' ? currentIndex + 1 : currentIndex - 1;
    return nextIndex >= 0 && nextIndex < numItems ? nextIndex : -1;
  }

  private dropdownValueSelected_(dropdownItem?: Element) {
    this.closeDropdown_();
    if (dropdownItem) {
      this.fireDropdownValueSelected_(dropdownItem);
    }
    this.shadowRoot!.querySelector<HTMLElement>(
                        '#destination-dropdown')!.focus();
  }

  /**
   * Returns list of all the visible items in the dropdown.
   */
  private getButtonListFromDropdown_(): HTMLButtonElement[] {
    if (!this.shadowRoot) {
      return [];
    }

    const dropdown = this.shadowRoot!.querySelector('iron-dropdown')!;
    return Array
        .from(dropdown.querySelectorAll<HTMLButtonElement>('.list-item'))
        .filter(item => !item.hidden);
  }

  /**
   * Sets tabindex to -1 when dropdown is disabled to prevent the dropdown from
   * being focusable.
   */
  private updateTabIndex_() {
    this.shadowRoot!.querySelector('#destination-dropdown')!.setAttribute(
        'tabindex', this.disabled ? '-1' : '0');
  }

  /**
   * Determines if an item in the dropdown should be highlighted based on the
   * current value of |highlightedIndex_|.
   */
  private getHighlightedClass_(itemValue: string): string {
    const itemToHighlight =
        this.getButtonListFromDropdown_()[this.highlightedIndex_];
    return itemToHighlight && itemValue === itemToHighlight.value ?
        'highlighted' :
        '';
  }

  /**
   * Close the dropdown when focus is lost except when an item in the dropdown
   * is the element that received the focus.
   */
  private onBlur_(event: FocusEvent) {
    if (!this.getButtonListFromDropdown_().includes(
            (event.relatedTarget as HTMLButtonElement))) {
      this.closeDropdown_();
    }
  }

  private computeDropdownLength_(): number {
    if (this.noDestinations) {
      return 1;
    }

    if (!this.itemList) {
      return 0;
    }

    // + 1 for "See more"
    let length = this.itemList.length + 1;
    if (!this.pdfPrinterDisabled) {
      length++;
    }
    if (this.driveDestinationKey) {
      length++;
    }
    return length;
  }

  private getPrinterStatusErrorString_(printerStatusReason:
                                           PrinterStatusReason): string {
    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18n(errorStringKey) : '';
  }

  private getPrinterStatusIcon_(
      printerStatusReason: PrinterStatusReason,
      isEnterprisePrinter: boolean): string {
    return getPrinterStatusIcon(printerStatusReason, isEnterprisePrinter);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-dropdown-cros':
        PrintPreviewDestinationDropdownCrosElement;
  }
}

customElements.define(
    PrintPreviewDestinationDropdownCrosElement.is,
    PrintPreviewDestinationDropdownCrosElement);

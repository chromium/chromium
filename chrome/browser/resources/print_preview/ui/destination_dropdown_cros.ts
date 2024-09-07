// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
// TODO(gavinwill): Remove iron-dropdown dependency https://crbug.com/1082587.
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './print_preview_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import type {PrinterStatusReason} from '../data/printer_status_cros.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon} from '../data/printer_status_cros.js';

import {getTemplate} from './destination_dropdown_cros.html.js';


declare global {
  interface HTMLElementEventMap {
    'dropdown-value-selected': CustomEvent<HTMLButtonElement>;
  }
}

export interface PrintPreviewDestinationDropdownCrosElement {
  $: {
    destinationDropdown: HTMLDivElement,
  };
}

const PrintPreviewDestinationDropdownCrosElementBase =
    I18nMixin(PolymerElement);

export class PrintPreviewDestinationDropdownCrosElement extends
    PrintPreviewDestinationDropdownCrosElementBase {
  static get is() {
    return 'print-preview-destination-dropdown-cros';
  }

  static get template() {
    return getTemplate();
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

      isDarkModeActive_: Boolean,

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

      pdfPosinset: Number,

      drivePosinset: Number,

      seeMorePosinset: Number,
    };
  }

  static get observers() {
    return [
      'updateAriaPosinset(itemList, pdfPrinterDisabled, driveDestinationKey)',
    ];
  }

  value: Destination;
  itemList: Destination[];
  destinationIcon: string;
  disabled: boolean;
  driveDestinationKey: string;
  noDestinations: boolean;
  pdfDestinationKey: string;
  pdfPrinterDisabled: boolean;
  destinationStatusText: TrustedHTML;
  private isDarkModeActive_: boolean;
  private highlightedIndex_: number;
  private dropdownLength_: number;
  private pdfPosinset: number;
  private drivePosinset: number;
  private seeMorePosinset: number;

  private opened_: boolean = false;
  private dropdownRefitPending_: boolean = false;

  override ready() {
    super.ready();

    this.addEventListener('mousemove', e => this.onMouseMove_(e));
  }

  override connectedCallback() {
    super.connectedCallback();

    this.updateTabIndex_();
  }

  override focus() {
    this.$.destinationDropdown.focus();
  }

  private getAriaDescription_(): string {
    return this.destinationStatusText.toString();
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
    switch (event.key) {
      case 'ArrowUp':
      case 'ArrowDown':
        this.onArrowKeyPress_(event.key);
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

  private onArrowKeyPress_(eventKey: string) {
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
          eventKey, this.highlightedIndex_, items.length);
      if (nextIndex === -1) {
        return;
      }
      this.highlightedIndex_ = nextIndex;
      items[this.highlightedIndex_].focus();
      return;
    }

    const currentIndex = items.findIndex(item => item.value === this.value.key);
    const nextIndex =
        this.getNextItemIndexInList_(eventKey, currentIndex, items.length);
    if (nextIndex === -1) {
      return;
    }
    this.fireDropdownValueSelected_(items[nextIndex]);
  }

  /**
   * @return -1 when the next item would be outside the list.
   */
  private getNextItemIndexInList_(
      eventKey: string, currentIndex: number, numItems: number): number {
    const nextIndex =
        eventKey === 'ArrowDown' ? currentIndex + 1 : currentIndex - 1;
    return nextIndex >= 0 && nextIndex < numItems ? nextIndex : -1;
  }

  private dropdownValueSelected_(dropdownItem?: Element) {
    this.closeDropdown_();
    if (dropdownItem) {
      this.fireDropdownValueSelected_(dropdownItem);
    }
    this.$.destinationDropdown.focus();
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
    this.$.destinationDropdown.setAttribute(
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
    return getPrinterStatusIcon(
        printerStatusReason, isEnterprisePrinter, this.isDarkModeActive_);
  }

  private getPrinterPosinset_(index: number): number {
    return index + 1;
  }

  /**
   * Set the ARIA position in the dropdown based on the visible items.
   */
  private updateAriaPosinset(): void {
    let currentPosition = this.itemList ? this.itemList.length + 1 : 1;
    if (!this.pdfPrinterDisabled) {
      this.pdfPosinset = currentPosition++;
    }
    if (this.driveDestinationKey) {
      this.drivePosinset = currentPosition++;
    }
    this.seeMorePosinset = currentPosition++;
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmark_row.html.js';

export class PowerBookmarkRowElement extends PolymerElement {
  static get is() {
    return 'power-bookmark-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bookmark: Object,
      checkboxDisabled: {
        type: Boolean,
        value: false,
      },
      compact: {
        type: Boolean,
        value: false,
      },
      description: {
        type: String,
        value: '',
      },
      hasCheckbox: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      hasInput: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      trailingIcon: {
        type: String,
        value: '',
      },
      trailingIconAriaLabel: {
        type: String,
        value: '',
      },
    };
  }

  bookmark: chrome.bookmarks.BookmarkTreeNode;
  checkboxDisabled: boolean;
  compact: boolean;
  description: string;
  hasCheckbox: boolean;
  hasInput: boolean;
  trailingIcon: string;
  trailingIconAriaLabel: string;

  override connectedCallback() {
    super.connectedCallback();
    this.onInputDisplayChange_();
  }

  private getItemSize_() {
    return this.compact ? CrUrlListItemSize.COMPACT : CrUrlListItemSize.LARGE;
  }

  private onInputDisplayChange_() {
    const input = this.shadowRoot!.querySelector('#input');
    if (input) {
      (input as CrInputElement).focus();
    }
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the row.
   */
  private onRowClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('row-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user right-clicks anywhere on the
   * row.
   */
  private onContextMenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('context-menu', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the
   * trailing icon button.
   */
  private onTrailingIconClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('trailing-icon-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user clicks on the checkbox.
   */
  private onCheckboxChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('checkbox-change', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        checked: (event.target as CrCheckboxElement).checked,
      },
    }));
  }

  /**
   * Triggers an input change event on enter. Extends default input behavior
   * which only triggers a change event if the value of the input has changed.
   */
  private onInputKeyPress_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onInputChange_(event);
    }
  }

  /**
   * Triggers a custom input change event when the user hits enter or the input
   * loses focus.
   */
  private onInputChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    const inputElement: CrInputElement =
        this.shadowRoot!.querySelector('#input')!;
    this.dispatchEvent(new CustomEvent('input-change', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        value: inputElement.value,
      },
    }));
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);

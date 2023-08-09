// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrUrlListItemElement, CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmark_row.html.js';

export interface PowerBookmarkRowElement {
  $: {
    crUrlListItem: CrUrlListItemElement,
  };
}

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
      checkboxChecked: {
        type: Boolean,
        value: false,
      },
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
      forceHover: {
        type: Boolean,
        value: false,
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
      imageUrls: {
        type: Array,
        value: () => [],
      },
      rowAriaDescription: {
        type: String,
        value: '',
      },
      rowAriaLabel: {
        type: String,
        value: '',
      },
      trailingIcon: {
        type: String,
        value: '',
      },
      trailingIconAriaLabel: {
        type: String,
        value: '',
      },
      trailingIconTooltip: {
        type: String,
        value: '',
      },
    };
  }

  bookmark: chrome.bookmarks.BookmarkTreeNode;
  checkboxChecked: boolean;
  checkboxDisabled: boolean;
  compact: boolean;
  description: string;
  forceHover: boolean;
  hasCheckbox: boolean;
  hasInput: boolean;
  rowAriaDescription: string;
  rowAriaLabel: string;
  trailingIcon: string;
  trailingIconAriaLabel: string;
  trailingIconTooltip: string;
  imageUrls: string[];

  override connectedCallback() {
    super.connectedCallback();
    this.onInputDisplayChange_();
    this.addEventListener('keydown', this.onKeydown_);
    this.addEventListener('focus', this.onFocus_);
  }

  override focus() {
    this.$.crUrlListItem.focus();
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.shadowRoot!.activeElement !== this.$.crUrlListItem) {
      return;
    }
    if (e.shiftKey && e.key === 'Tab') {
      // Hitting shift tab from CrUrlListItem to traverse focus backwards will
      // attempt to move focus to this element, which is responsible for
      // delegating focus but should itself not be focusable. So when the user
      // hits shift tab, immediately hijack focus onto itself so that the
      // browser moves focus to the focusable element before it once it
      // processes the shift tab.
      super.focus();
    } else if (e.key === 'Enter') {
      // Prevent iron-list from moving focus.
      e.stopPropagation();
    }
  }

  private onFocus_(e: FocusEvent) {
    if (e.composedPath()[0] === this && this.matches(':focus-visible')) {
      // If trying to directly focus on this row, move the focus to the
      // <cr-url-list-item>. Otherwise, UI might be trying to directly focus on
      // a specific child (eg. the input).
      // This should only be done when focusing via keyboard, to avoid blocking
      // drag interactions.
      this.$.crUrlListItem.focus();
    }
  }

  private getItemSize_() {
    return this.compact ? CrUrlListItemSize.COMPACT : CrUrlListItemSize.LARGE;
  }

  private isBookmarksBar_(): boolean {
    return this.bookmark.id === loadTimeData.getString('bookmarksBarId');
  }

  private showTrailingIcon_(): boolean {
    return !this.hasInput && !this.hasCheckbox;
  }

  private onInputDisplayChange_() {
    const input = this.shadowRoot!.querySelector<CrInputElement>('#input');
    if (input) {
      input.select();
    }
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the row.
   */
  private onRowClicked_(event: MouseEvent) {
    // Ignore clicks on the row when it has an input, to ensure the row doesn't
    // eat input clicks. Also ignore clicks if the row has no associated
    // bookmark, or if the event is a right-click.
    if (this.hasInput || !this.bookmark || event.button === 2) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    if (this.hasCheckbox && !this.checkboxDisabled) {
      // Clicking the row should trigger a checkbox click rather than a
      // standard row click.
      const checkbox =
          this.shadowRoot!.querySelector<CrCheckboxElement>('#checkbox')!;
      checkbox.checked = !checkbox.checked;
      return;
    }
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
  private onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.onInputChange_(event);
    }
  }

  private createInputChangeEvent_(value: string|null) {
    return new CustomEvent('input-change', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        value: value,
      },
    });
  }

  /**
   * Triggers a custom input change event when the user hits enter or the input
   * loses focus.
   */
  private onInputChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    const inputElement =
        this.shadowRoot!.querySelector<CrInputElement>('#input')!;
    this.dispatchEvent(this.createInputChangeEvent_(inputElement.value));
  }

  private onInputBlur_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(this.createInputChangeEvent_(null));
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-searchable-drop-down' implements a search box with a
 * suggestions drop down.
 *
 * If the update-value-on-input flag is set, value will be set to whatever is
 * in the input box. Otherwise, value will only be set when an element in items
 * is clicked.
 *
 * The |invalid| property tracks whether the user's current text input in the
 * dropdown matches the previously saved dropdown value. This property can be
 * used to disable certain user actions when the dropdown is invalid.
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_searchable_drop_down/
 *     cr_searchable_drop_down.ts
 */
import '../cr_input/cr_input.js';
import '../cr_hidden_style.css.js';
import '../icons.html.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {IronDropdownElement} from '//resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrInputElement} from '../cr_input/cr_input.js';

import {getTemplate} from './cr_searchable_drop_down.html.js';

export interface CrSearchableDropDownElement {
  $: {
    search: CrInputElement,
    dropdown: IronDropdownElement,
  };
}

export class CrSearchableDropDownElement extends PolymerElement {
  static get is() {
    return 'cr-searchable-drop-down';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      autofocus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      readonly: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * Whether space should be left below the text field to display an error
       * message. Must be true for |errorMessage| to be displayed.
       */
      errorMessageAllowed: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * When |errorMessage| is set, the text field is highlighted red and
       * |errorMessage| is displayed beneath it.
       */
      errorMessage: String,

      /**
       * Message to display next to the loading spinner.
       */
      loadingMessage: String,

      placeholder: String,

      /**
       * Used to track in real time if the |value| in cr-searchable-drop-down
       * matches the value in the underlying cr-input. These values will differ
       * after a user types in input that does not match a valid dropdown
       * option. |invalid| is always false when |updateValueOnInput| is set to
       * true. This is because when |updateValueOnInput| is set to true, we are
       * not setting a restrictive set of valid options.
       */
      invalid: {
        type: Boolean,
        value: false,
        notify: true,
      },

      items: {
        type: Array,
        observer: 'onItemsChanged_',
      },

      value: {
        type: String,
        notify: true,
        observer: 'updateInvalid_',
      },

      label: {
        type: String,
        value: '',
      },

      updateValueOnInput: Boolean,

      showLoading: {
        type: Boolean,
        value: false,
      },

      searchTerm_: String,

      dropdownRefitPending_: Boolean,

      /**
       * Whether the dropdown is currently open. Should only be used by CSS
       * privately.
       */
      opened_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  override autofocus: boolean;
  readonly: boolean;
  errorMessageAllowed: boolean;
  errorMessage: string;
  loadingMessage: string;
  placeholder: string;
  invalid: boolean;
  items: string[];
  value: string;
  label: string;
  updateValueOnInput: boolean;
  showLoading: boolean;

  private searchTerm_: string;
  private dropdownRefitPending_: boolean;
  private opened_: boolean;
  private openDropdownTimeoutId_: number = 0;
  private resizeObserver_: ResizeObserver|null = null;
  private pointerDownListener_: (e: Event) => void;

  override connectedCallback() {
    super.connectedCallback();

    this.pointerDownListener_ = this.onPointerDown_.bind(this);
    document.addEventListener('pointerdown', this.pointerDownListener_);
    this.resizeObserver_ = new ResizeObserver(() => {
      this.resizeDropdown_();
    });
    this.resizeObserver_.observe(this.$.search);
  }

  override ready() {
    super.ready();
    this.addEventListener('mousemove', this.onMouseMove_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    document.removeEventListener('pointerdown', this.pointerDownListener_);
    this.resizeObserver_!.unobserve(this.$.search);
  }

  /**
   * Enqueues a task to refit the iron-dropdown if it is open.
   */
  private enqueueDropdownRefit_() {
    const dropdown = this.$.dropdown;
    if (!this.dropdownRefitPending_ && dropdown.opened) {
      this.dropdownRefitPending_ = true;
      setTimeout(() => {
        dropdown.refit();
        this.dropdownRefitPending_ = false;
      }, 0);
    }
  }

  /**
   * Keeps the dropdown from expanding beyond the width of the search input when
   * its width is specified as a percentage.
   */
  private resizeDropdown_() {
    const dropdown = this.$.dropdown.containedElement;
    const dropdownWidth =
        Math.max(dropdown.offsetWidth, this.$.search.offsetWidth);
    dropdown.style.width = `${dropdownWidth}px`;
    this.enqueueDropdownRefit_();
  }

  private openDropdown_() {
    this.$.dropdown.open();
    this.opened_ = true;
  }

  private closeDropdown_() {
    if (this.openDropdownTimeoutId_) {
      clearTimeout(this.openDropdownTimeoutId_);
    }

    this.$.dropdown.close();
    this.opened_ = false;
  }

  /**
   * Enqueues a task to open the iron-dropdown. Any pending task is canceled and
   * a new task is enqueued.
   */
  private enqueueOpenDropdown_() {
    if (this.opened_) {
      return;
    }
    if (this.openDropdownTimeoutId_) {
      clearTimeout(this.openDropdownTimeoutId_);
    }
    this.openDropdownTimeoutId_ = setTimeout(this.openDropdown_.bind(this));
  }

  private onItemsChanged_() {
    // Refit the iron-dropdown so that it can expand as neccessary to
    // accommodate new items. Refitting is done on a new task because the change
    // notification might not yet have propagated to the iron-dropdown.
    this.enqueueDropdownRefit_();
  }

  private onFocus_() {
    if (this.readonly) {
      return;
    }
    this.openDropdown_();
  }

  private onMouseMove_(event: Event) {
    const item = event.composedPath().find(elm => {
      const element = elm as HTMLElement;
      return element.classList && element.classList.contains('list-item');
    }) as HTMLElement |
        undefined;

    if (!item) {
      return;
    }

    // Select the item the mouse is hovering over. If the user uses the
    // keyboard, the selection will shift. But once the user moves the mouse,
    // selection should be updated based on the location of the mouse cursor.
    const selectedItem = this.findSelectedItem_();
    if (item === selectedItem) {
      return;
    }

    if (selectedItem) {
      selectedItem.removeAttribute('selected_');
    }
    item.setAttribute('selected_', '');
  }

  private onPointerDown_(event: Event) {
    if (this.readonly) {
      return;
    }

    const paths = event.composedPath();
    const searchInput = this.$.search.inputElement;
    if (paths.includes(this.$.dropdown)) {
      // At this point, the search input field has lost focus. Since the user
      // is still interacting with this element, give the search field focus.
      searchInput.focus();
      // Prevent any other field from gaining focus due to this event.
      event.preventDefault();
    } else if (paths.includes(searchInput)) {
      // A click on the search input should open the dropdown. Opening the
      // dropdown is done on a new task because when the IronDropdown element is
      // opened, it may capture and cancel the touch event, preventing the
      // searchInput field from receiving focus. Replacing iron-dropdown
      // (crbug.com/1013408) will eliminate the need for this work around.
      this.enqueueOpenDropdown_();
    } else {
      // A click outside either the search input or dropdown should close the
      // dropdown. Implicitly, the search input has lost focus at this point.
      this.closeDropdown_();
    }
  }

  private onKeyDown_(event: KeyboardEvent) {
    const dropdown = this.$.dropdown;
    if (!dropdown.opened) {
      if (this.readonly) {
        return;
      }
      if (event.key === 'Enter') {
        this.openDropdown_();
        // Stop the default submit action.
        event.preventDefault();
      }
      return;
    }

    event.stopPropagation();
    switch (event.key) {
      case 'Tab':
        // Pressing tab will cause the input field to lose focus. Since the
        // dropdown visibility is tied to focus, close the dropdown.
        this.closeDropdown_();
        break;
      case 'ArrowUp':
      case 'ArrowDown': {
        const selected = this.findSelectedItemIndex_();
        const items = dropdown.querySelectorAll<HTMLElement>('.list-item');
        if (items.length === 0) {
          break;
        }
        this.updateSelected_(items, selected, event.key === 'ArrowDown');
        break;
      }
      case 'Enter': {
        const selected = this.findSelectedItem_();
        if (!selected) {
          break;
        }
        selected.removeAttribute('selected_');
        this.value = (dropdown.querySelector('dom-repeat')!.modelForElement(
                          selected) as unknown as {
                       item: string,
                     }).item;
        this.searchTerm_ = '';
        this.closeDropdown_();
        // Stop the default submit action.
        event.preventDefault();
        break;
      }
    }
  }

  /**
   * Finds the currently selected dropdown item.
   * @return Currently selected dropdown item, or undefined if no item is
   *     selected.
   */
  private findSelectedItem_(): HTMLElement|undefined {
    const items =
        Array.from(this.$.dropdown.querySelectorAll<HTMLElement>('.list-item'));
    return items.find(item => item.hasAttribute('selected_'));
  }

  /**
   * Finds the index of currently selected dropdown item.
   * @return Index of the currently selected dropdown item, or -1 if no item is
   *     selected.
   */
  private findSelectedItemIndex_(): number {
    const items =
        Array.from(this.$.dropdown.querySelectorAll<HTMLElement>('.list-item'));
    return items.findIndex(item => item.hasAttribute('selected_'));
  }

  /**
   * Updates the currently selected element based on keyboard up/down movement.
   */
  private updateSelected_(
      items: NodeListOf<HTMLElement>, currentIndex: number, moveDown: boolean) {
    const numItems = items.length;
    let nextIndex = 0;
    if (currentIndex === -1) {
      nextIndex = moveDown ? 0 : numItems - 1;
    } else {
      const delta = moveDown ? 1 : -1;
      nextIndex = (numItems + currentIndex + delta) % numItems;
      items[currentIndex]!.removeAttribute('selected_');
    }
    items[nextIndex]!.setAttribute('selected_', '');
    // The newly selected item might not be visible because the dropdown needs
    // to be scrolled. So scroll the dropdown if necessary.
    items[nextIndex]!.scrollIntoViewIfNeeded();
  }

  private onInput_() {
    this.searchTerm_ = this.$.search.value;

    if (this.updateValueOnInput) {
      this.value = this.$.search.value;
    }

    // If the user makes a change, ensure the dropdown is open. The dropdown is
    // closed when the user makes a selection using the mouse or keyboard.
    // However, focus remains on the input field. If the user makes a further
    // change, then the dropdown should be shown.
    this.openDropdown_();

    // iron-dropdown sets its max-height when it is opened. If the current value
    // results in no filtered items in the drop down list, the iron-dropdown
    // will have a max-height for 0 items. If the user then clears the input
    // field, a non-zero number of items might be displayed in the drop-down,
    // but the height is still limited based on 0 items. This results in a tiny,
    // but scollable dropdown. Refitting the dropdown allows it to expand to
    // accommodate the new items.
    this.enqueueDropdownRefit_();

    // Need check to if the input is valid when the user types.
    this.updateInvalid_();
  }

  private onSelect_(event: DomRepeatEvent<string>) {
    this.closeDropdown_();

    this.value = event.model.item;
    this.searchTerm_ = '';

    const selected = this.findSelectedItem_();
    if (selected) {
      // Reset the selection state.
      selected.removeAttribute('selected_');
    }
  }

  private filterItems_(searchTerm: string): ((s: string) => boolean)|null {
    if (!searchTerm) {
      return null;
    }
    return function(item) {
      return item.toLowerCase().includes(searchTerm.toLowerCase());
    };
  }

  private shouldShowErrorMessage_(
      errorMessage: string, errorMessageAllowed: boolean): boolean {
    return !!this.getErrorMessage_(errorMessage, errorMessageAllowed);
  }

  private getErrorMessage_(errorMessage: string, errorMessageAllowed: boolean):
      string {
    if (!errorMessageAllowed) {
      return '';
    }
    return errorMessage;
  }

  /**
   * This makes sure to reset the text displayed in the dropdown to the actual
   * value in the cr-input for the use case where a user types in an invalid
   * option then changes focus from the dropdown. This behavior is only for when
   * updateValueOnInput is false. When updateValueOnInput is true, it is ok to
   * leave the user's text in the dropdown search bar when focus is changed.
   */
  private onBlur_() {
    if (!this.updateValueOnInput) {
      this.$.search.value = this.value;
    }

    // Need check to if the input is valid when the dropdown loses focus.
    this.updateInvalid_();
  }

  /**
   * If |updateValueOnInput| is true then any value is allowable so always set
   * |invalid| to false.
   */
  private updateInvalid_() {
    this.invalid =
        !this.updateValueOnInput && (this.value !== this.$.search.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchable-drop-down': CrSearchableDropDownElement;
  }
}

customElements.define(
    CrSearchableDropDownElement.is, CrSearchableDropDownElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {afterNextRender, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_chrome_combobox.html.js';

/* Selector for keyboard focusable items in the dropdown. */
const HIGHLIGHTABLE_ITEMS_SELECTOR = '[role=group] > label, [role=option]';

/* Selector for selectable options in the dropdown. */
const SELECTABLE_ITEMS_SELECTOR = '[role=option]';

export type OptionElement = HTMLElement&{value?: string};

export interface ComboboxItem {
  label: string;
  imagePath?: string;
}

export interface ComboboxGroup {
  label: string;
  items: ComboboxItem[];
}

/* Running count of total items. Incremented to provide unique IDs. */
let itemCount = 0;

export interface CustomizeChromeCombobox {
  $: {
    input: HTMLDivElement,
    dropdown: HTMLDivElement,
  };
}

export class CustomizeChromeCombobox extends PolymerElement {
  static get is() {
    return 'customize-chrome-combobox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      defaultOptionLabel: String,
      expanded_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'onExpandedChange_',
      },
      expandedGroups_: Object,
      highlightedElement_: Object,
      indentDefaultOption_: {
        type: Boolean,
        computed: 'computeIndentDefaultOption_(items)',
        reflectToAttribute: true,
      },
      items: {
        type: Array,
        value: () => [],
      },
      label: String,
      rightAlignDropbox: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      selectedElement_: {
        type: Object,
        observer: 'onSelectedElementChanged_',
      },
      value: {
        type: String,
        notify: true,
        observer: 'selectItemFromValue_',
      },
    };
  }

  defaultOptionLabel: string;
  private expanded_: boolean;
  private expandedGroups_: {[groupIndex: number]: boolean} = {};
  private highlightableElements_: HTMLElement[] = [];
  private highlightedElement_: HTMLElement|null = null;
  private indentDefaultOption_: boolean;
  items: ComboboxGroup[]|ComboboxItem[];
  label: string;
  private lastHighlightWasByKeyboard_: boolean = false;
  private domObserver_: MutationObserver|null = null;
  private selectedElement_: OptionElement|null = null;
  value: string|undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('keydown', this.onKeydown_.bind(this));

    // Listen for changes in the component's DOM to grab list of selectable
    // elements. Note that a slotchange event does not work here since
    // slotchange only listens for changes to direct children of the component.
    this.domObserver_ = new MutationObserver(this.onDomChange_.bind(this));
    this.domObserver_.observe(
        this.$.dropdown, {attributes: false, childList: true, subtree: true});

    // Call the observer's callback once to initialize.
    this.onDomChange_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.domObserver_?.disconnect();
    this.domObserver_ = null;
  }

  // The default option needs to be indented with extra padding if it sits
  // right above an option that is not a group and does not have an image as
  // these items have extra space for a checkmark icon.
  private computeIndentDefaultOption_(): boolean {
    if (this.items.length === 0) {
      return false;
    }

    const firstItem = this.items[0];
    if ('items' in firstItem) {
      // First item is a group, so not indented.
      return false;
    }

    // Only indent if there is no image in the first item.
    return !('imagePath' in firstItem);
  }

  private getAriaActiveDescendant_(): string|undefined {
    return this.highlightedElement_?.id;
  }

  private getDefaultItemAriaSelected_(): string {
    return this.value === undefined ? 'true' : 'false';
  }

  private getGroupAriaExpanded_(groupIndex: number): string {
    return this.expandedGroups_[groupIndex] ? 'true' : 'false';
  }

  private getGroupIcon_(groupIndex: number): string {
    return this.expandedGroups_[groupIndex] ? 'cr:expand-less' :
                                              'cr:expand-more';
  }

  private getInputLabel_(): string {
    if (this.selectedElement_ && this.selectedElement_.value &&
        this.selectedElement_.value === this.value) {
      return this.selectedElement_.textContent!;
    }

    return this.label;
  }

  private getItemAriaSelected_(item: ComboboxItem) {
    return this.isItemSelected_(item) ? 'true' : 'false';
  }

  private highlightElement_(element: HTMLElement|null, byKeyboard: boolean) {
    if (this.highlightedElement_) {
      this.highlightedElement_.removeAttribute('highlighted');
    }

    if (element) {
      element.toggleAttribute('highlighted', true);

      if (byKeyboard) {
        element.scrollIntoView({block: 'nearest'});
      }
    }

    this.highlightedElement_ = element;
    this.lastHighlightWasByKeyboard_ = byKeyboard;
  }

  private isGroup_(item: ComboboxGroup|ComboboxItem): boolean {
    return item.hasOwnProperty('items');
  }

  private isGroupExpanded_(groupIndex: number): boolean {
    return this.expandedGroups_[groupIndex];
  }

  private isItemSelected_(item: ComboboxItem): boolean {
    return this.value === item.label;
  }

  private onDomChange_() {
    this.highlightableElements_ =
        Array.from(this.shadowRoot!.querySelectorAll<HTMLElement>(
            HIGHLIGHTABLE_ITEMS_SELECTOR));

    this.highlightableElements_.forEach(element => {
      if (!element.id) {
        element.id = `comboboxItem${itemCount++}`;
      }
    });

    if (this.value) {
      this.selectItemFromValue_();
    }
  }

  private onDropdownClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();

    const selectableTarget =
        event.composedPath().find(
            target => target instanceof HTMLElement &&
                target.matches(SELECTABLE_ITEMS_SELECTOR)) as HTMLElement;
    if (!selectableTarget) {
      return;
    }
    this.selectItem_(selectableTarget);
    this.expanded_ = false;
  }

  private onDropdownPointerdown_(e: PointerEvent) {
    /* Prevent the dropdown from gaining focus on pointerdown. The input should
     * always be the focused element. */
    e.preventDefault();
  }

  private onDropdownPointerevent_(event: PointerEvent) {
    const highlightableTarget =
        event.composedPath().find(
            target => target instanceof HTMLElement &&
                target.matches(HIGHLIGHTABLE_ITEMS_SELECTOR)) as HTMLElement;
    if (!highlightableTarget ||
        this.highlightedElement_ === highlightableTarget) {
      return;
    }

    this.highlightElement_(highlightableTarget, false);
  }

  private onDropdownPointermove_(event: PointerEvent) {
    if (!this.lastHighlightWasByKeyboard_) {
      // Ignore any pointermove events if the last highlight was done by
      // pointer. This is to avoid re-calculating a potentially highlighted item
      // any time the pointer moves within an item.
      return;
    }

    this.onDropdownPointerevent_(event);
  }

  private onDropdownPointerover_(event: PointerEvent) {
    if (this.lastHighlightWasByKeyboard_) {
      // Ignore pointerover events if the last highlight was done by keyboard,
      // as pointermove events should catch any pointer-related events. This
      // also avoids cases where a pointerover event is fired when a keyboard
      // highlight causes the dropdown to scroll, leading to the pointer
      // being over a new element.
      return;
    }

    this.onDropdownPointerevent_(event);
  }

  private onExpandedChange_() {
    this.highlightElement_(this.selectedElement_, false);
  }

  private onGroupClick_(e: DomRepeatEvent<ComboboxGroup>) {
    const index = e.model.index;
    this.set(`expandedGroups_.${index}`, !this.expandedGroups_[index]);
  }

  private onInputClick_() {
    this.expanded_ = !this.expanded_;
  }

  private onInputFocusout_() {
    this.expanded_ = false;
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.expanded_) {
      this.onKeydownExpandedState_(e);
    } else {
      this.onKeydownCollapsedState_(e);
    }
  }

  private onKeydownCollapsedState_(e: KeyboardEvent) {
    if (!['ArrowDown', 'ArrowUp', 'Home', 'End', 'Enter', 'Space'].includes(
            e.key)) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    this.expanded_ = true;
    if (this.highlightedElement_) {
      // If an item is already highlighted, nothing to do.
      return;
    }

    // Highlight the first item for most keys, unless the key is ArrowUp/End.
    let elementToHighlight = this.highlightableElements_[0];
    if (e.key === 'ArrowUp' || e.key === 'End') {
      elementToHighlight =
          this.highlightableElements_[this.highlightableElements_.length - 1];
    }

    if (elementToHighlight) {
      this.highlightElement_(elementToHighlight, true);
    }
  }

  private onKeydownExpandedState_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      this.expanded_ = false;
      return;
    }

    if (e.key === 'Enter' || e.key === 'Space') {
      e.preventDefault();
      e.stopPropagation();
      if (this.selectItem_(this.highlightedElement_)) {
        this.expanded_ = false;
      }
      return;
    }

    if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    let index = this.highlightedElement_ ?
        this.highlightableElements_.indexOf(this.highlightedElement_) :
        -1;
    switch (e.key) {
      case 'ArrowDown':
        index++;
        break;
      case 'ArrowUp':
        index--;
        break;
      case 'Home':
        index = 0;
        break;
      case 'End':
        index = this.highlightableElements_.length - 1;
        break;
    }

    if (index < 0) {
      index = this.highlightableElements_.length - 1;
    } else if (index > this.highlightableElements_.length - 1) {
      index = 0;
    }

    this.highlightElement_(this.highlightableElements_[index]!, true);
  }

  private onSelectedElementChanged_() {
    if (!this.selectedElement_) {
      this.value = undefined;
      return;
    }

    this.value = this.selectedElement_.value;
  }

  private selectItemFromValue_() {
    if (!this.value) {
      return;
    }

    if (this.selectedElement_?.isConnected &&
        this.selectedElement_.value === this.value) {
      // Selected element matches the value. Nothing left to do.
      return;
    }

    const selectedGroupIndex =
        this.items.filter(item => this.isGroup_(item)).findIndex((group) => {
          return (group as ComboboxGroup)
              .items.find((item) => item.label === this.value);
        });
    if (selectedGroupIndex > -1) {
      this.set(`expandedGroups_.${selectedGroupIndex}`, true);
    }

    afterNextRender(this, () => {
      this.selectItem_(
          (Array.from(this.shadowRoot!.querySelectorAll(
               SELECTABLE_ITEMS_SELECTOR)) as OptionElement[])
              .find(option => option.value === this.value) ||
          null);
    });
  }

  private selectItem_(item: HTMLElement|null): boolean {
    if (!item) {
      return false;
    }

    if (!item.matches(SELECTABLE_ITEMS_SELECTOR)) {
      item.click();
      return false;
    }

    if (this.selectedElement_) {
      this.selectedElement_.removeAttribute('selected');
    }

    item.toggleAttribute('selected', true);
    this.selectedElement_ = item as OptionElement;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-combobox': CustomizeChromeCombobox;
  }
}

customElements.define(CustomizeChromeCombobox.is, CustomizeChromeCombobox);

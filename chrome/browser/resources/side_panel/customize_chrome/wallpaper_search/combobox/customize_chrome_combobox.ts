// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../check_mark_wrapper.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './customize_chrome_combobox.css.js';
import {getHtml} from './customize_chrome_combobox.html.js';

/* Selector for keyboard focusable items in the dropdown. */
const HIGHLIGHTABLE_ITEMS_SELECTOR = '[role=group] > label, [role=option]';

/* Selector for selectable options in the dropdown. */
const SELECTABLE_ITEMS_SELECTOR = '[role=option]';

export type OptionElement = HTMLElement&{value?: string};

export interface ComboboxItem {
  key: string;
  label: string;
  imagePath?: string;
}

export interface ComboboxGroup {
  key: string;
  label: string;
  items: ComboboxItem[];
}

/* Running count of total items. Incremented to provide unique IDs. */
let itemCount = 0;

export interface CustomizeChromeComboboxElement {
  $: {
    input: HTMLDivElement,
    dropdown: HTMLDivElement,
  };
}

export class CustomizeChromeComboboxElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-combobox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      defaultOptionLabel: {type: String},
      expanded_: {
        type: Boolean,
        reflect: true,
      },
      expandedGroups_: {type: Object},
      highlightedElement_: {type: Object},
      indentDefaultOption_: {
        type: Boolean,
        reflect: true,
      },
      items: {type: Array},
      label: {type: String},
      rightAlignDropbox: {
        type: Boolean,
        reflect: true,
      },
      selectedElement_: {type: Object},
      value: {
        type: String,
        notify: true,
      },
    };
  }

  defaultOptionLabel: string = '';
  protected expanded_: boolean = false;
  private expandedGroups_: {[groupIndex: number]: boolean} = {};
  private highlightableElements_: HTMLElement[] = [];
  private highlightedElement_: HTMLElement|null = null;
  protected indentDefaultOption_: boolean = false;
  items: ComboboxGroup[]|ComboboxItem[] = [];
  label: string = '';
  rightAlignDropbox: boolean = false;
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('items')) {
      this.indentDefaultOption_ = this.computeIndentDefaultOption_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('selectedElement_')) {
      this.onSelectedElementChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('value')) {
      this.selectItemFromValue_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('expanded_')) {
      this.onExpandedChange_();
    }
  }

  // The default option needs to be indented with extra padding if it sits
  // right above an option that is not a group and does not have an image as
  // these items have extra space for a checkmark icon.
  private computeIndentDefaultOption_(): boolean {
    if (this.items.length === 0) {
      return false;
    }

    const firstItem = this.items[0]!;
    if ('items' in firstItem) {
      // First item is a group, so not indented.
      return false;
    }

    // Only indent if there is no image in the first item.
    return !('imagePath' in firstItem);
  }

  protected getAriaActiveDescendant_(): string|undefined {
    return this.highlightedElement_?.id;
  }

  protected getDefaultItemAriaSelected_(): string {
    return this.value === undefined ? 'true' : 'false';
  }

  protected getGroupAriaExpanded_(groupIndex: number): string {
    return this.expandedGroups_[groupIndex] ? 'true' : 'false';
  }

  protected getGroupIcon_(groupIndex: number): string {
    return this.expandedGroups_[groupIndex] ? 'cr:expand-less' :
                                              'cr:expand-more';
  }

  protected getInputLabel_(): string {
    if (this.selectedElement_ && this.selectedElement_.value &&
        this.selectedElement_.value === this.value) {
      return this.selectedElement_.textContent!;
    }

    return this.label;
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

  protected isGroup_(item: ComboboxGroup|ComboboxItem): boolean {
    return item.hasOwnProperty('items');
  }

  protected isGroupExpanded_(groupIndex: number): boolean {
    return this.expandedGroups_[groupIndex]!;
  }

  protected isItemSelected_(item: ComboboxItem): boolean {
    return this.value === item.key;
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

  protected onDropdownClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();

    const selectableTarget =
        event.composedPath().find(
            target => target instanceof HTMLElement &&
                target.matches(SELECTABLE_ITEMS_SELECTOR)) as HTMLElement;
    if (!selectableTarget) {
      return;
    }

    if (this.selectedElement_ === selectableTarget) {
      this.unselectSelectedItem_();
    } else {
      this.selectItem_(selectableTarget);
      this.expanded_ = false;
    }
  }

  protected onDropdownPointerdown_(e: PointerEvent) {
    /* Prevent the dropdown from gaining focus on pointerdown. The input should
     * always be the focused element. */
    e.preventDefault();
  }

  protected onDropdownPointerevent_(event: PointerEvent) {
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

  protected onDropdownPointermove_(event: PointerEvent) {
    if (!this.lastHighlightWasByKeyboard_) {
      // Ignore any pointermove events if the last highlight was done by
      // pointer. This is to avoid re-calculating a potentially highlighted item
      // any time the pointer moves within an item.
      return;
    }

    this.onDropdownPointerevent_(event);
  }

  protected onDropdownPointerover_(event: PointerEvent) {
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

  protected onGroupClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.expandedGroups_[index] = !this.expandedGroups_[index];
    this.requestUpdate();
  }

  protected onInputClick_() {
    this.expanded_ = !this.expanded_;
  }

  protected onInputFocusout_() {
    this.expanded_ = false;
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.expanded_) {
      this.onKeydownExpandedState_(e);
    } else {
      this.onKeydownCollapsedState_(e);
    }
  }

  private async onKeydownCollapsedState_(e: KeyboardEvent) {
    if (!['ArrowDown', 'ArrowUp', 'Home', 'End', 'Enter', 'Space'].includes(
            e.key)) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    this.expanded_ = true;
    await this.updateComplete;

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
      if (this.selectedElement_ === this.highlightedElement_) {
        this.unselectSelectedItem_();
      } else if (this.selectItem_(this.highlightedElement_)) {
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

  private async selectItemFromValue_() {
    if (!this.value) {
      return;
    }

    if (this.selectedElement_?.isConnected &&
        this.selectedElement_.value === this.value) {
      // Selected element matches the value. Nothing left to do.
      return;
    }

    const selectedGroupIndex =
        this.items.filter(item => this.isGroup_(item)).findIndex(group => {
          return (group as ComboboxGroup)
              .items.find(item => item.key === this.value);
        });
    if (selectedGroupIndex > -1) {
      this.expandedGroups_[selectedGroupIndex] = true;
      this.requestUpdate();
    }

    await this.updateComplete;
    this.selectItem_(
        Array
            .from(this.shadowRoot!.querySelectorAll<OptionElement>(
                SELECTABLE_ITEMS_SELECTOR))
            .find(option => option.value === this.value) ||
        null);
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

  private unselectSelectedItem_() {
    if (!this.selectedElement_) {
      return;
    }

    this.selectedElement_.removeAttribute('selected');
    this.selectedElement_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-combobox': CustomizeChromeComboboxElement;
  }
}

customElements.define(
    CustomizeChromeComboboxElement.is, CustomizeChromeComboboxElement);

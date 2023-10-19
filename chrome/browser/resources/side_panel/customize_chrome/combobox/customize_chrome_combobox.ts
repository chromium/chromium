// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_chrome_combobox.html.js';

/* Selector for keyboard focusable items in the dropdown. */
const HIGHLIGHTABLE_ITEMS_SELECTOR = '[role=group], [role=option]';

/* Selector for selectable options in the dropdown. */
const SELECTABLE_ITEMS_SELECTOR = '[role=option]';

export type OptionElement = HTMLElement&{value?: string};

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
      expanded_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'onExpandedChange_',
      },
      highlightedElement_: Object,
      label: String,
      selectedElement_: {
        type: Object,
        observer: 'onSelectedElementChanged_',
      },
      value: {
        type: String,
        notify: true,
        observer: 'onValueChanged_',
      },
    };
  }

  private expanded_: boolean;
  private highlightableElements_: HTMLElement[] = [];
  private highlightedElement_: HTMLElement|null = null;
  label: string;
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
    this.domObserver_.observe(this, {childList: true, subtree: true});

    // Call the observer's callback once to initialize.
    this.onDomChange_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.domObserver_?.disconnect();
    this.domObserver_ = null;
  }

  private getAriaActiveDescendant_(): string|undefined {
    return this.highlightedElement_?.id;
  }

  private getInputLabel_(): string {
    if (this.selectedElement_) {
      return this.selectedElement_.textContent!;
    }

    return this.label;
  }

  private highlightElement_(element: HTMLElement|null) {
    if (this.highlightedElement_) {
      this.highlightedElement_.removeAttribute('highlighted');
    }

    if (element) {
      element.toggleAttribute('highlighted', true);
    }

    this.highlightedElement_ = element;
  }

  private onDomChange_() {
    this.highlightableElements_ = Array.from(
        this.querySelectorAll<HTMLElement>(HIGHLIGHTABLE_ITEMS_SELECTOR));

    this.highlightableElements_.forEach(element => {
      if (!element.id) {
        element.id = `comboboxItem${itemCount++}`;
      }
    });
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

  private onExpandedChange_() {
    this.highlightElement_(this.selectedElement_);
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
      this.highlightElement_(elementToHighlight);
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

    this.highlightElement_(this.highlightableElements_[index]!);
  }

  private onSelectedElementChanged_() {
    if (!this.selectedElement_) {
      this.value = undefined;
      return;
    }

    this.value = this.selectedElement_.value;
  }

  private onValueChanged_() {
    if (!this.value) {
      return;
    }

    if (this.selectedElement_ && this.selectedElement_.value === this.value) {
      // Selected element matches the value. Nothing left to do.
      return;
    }

    this.selectItem_(
        (Array.from(this.querySelectorAll(SELECTABLE_ITEMS_SELECTOR)) as
         OptionElement[])
            .find(option => option.value === this.value) ||
        null);
  }

  private selectItem_(item: HTMLElement|null): boolean {
    if (!item) {
      return false;
    }

    if (!item.matches(SELECTABLE_ITEMS_SELECTOR)) {
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_chrome_combobox.html.js';

/* Selector for keyboard focusable items in the dropdown. */
const HIGHLIGHTABLE_ITEMS_SELECTOR = '[role=group], [role=option]';

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
      label: String,
    };
  }

  private expanded_: boolean;
  private highlightableElements_: HTMLElement[] = [];
  private highlightedElement_: HTMLElement|null = null;
  label: string;
  private domObserver_: MutationObserver|null = null;

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
  }

  private onExpandedChange_() {
    this.highlightElement_(null);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-combobox': CustomizeChromeCombobox;
  }
}

customElements.define(CustomizeChromeCombobox.is, CustomizeChromeCombobox);

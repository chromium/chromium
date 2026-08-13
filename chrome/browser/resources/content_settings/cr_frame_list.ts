// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';

import sheet from './cr_frame_list.css' with {type : 'css'};
import {getTemplate} from './cr_frame_list.html.js';

declare global {
  interface HTMLElementEventMap {
    'selected-index-change': CustomEvent<number>;
  }
}

export class CrFrameListElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  static get observedAttributes() {
    return ['selected-index'];
  }

  private tabs_: HTMLSlotElement;
  private focusOutlineManager_: FocusOutlineManager;

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
    this.tabs_ = this.getRequiredElement<HTMLSlotElement>('slot[name=tab]');
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
  }

  connectedCallback() {
    this.setupEventListeners();
    this.initializeOrProcessTabs();
  }

  /**
   * Sets up tabs: selects the active tab.
   */
  initializeOrProcessTabs() {
    const tabs = this.getSlottedTabs_();
    if (tabs.length === 0) {
      return;
    }

    const foundIndex = tabs.findIndex(tab => tab.hasAttribute('selected'));
    const selectedIndex = foundIndex !== -1 ? foundIndex : 0;

    if (!this.hasAttribute('selected-index')) {
      this.setAttribute('selected-index', selectedIndex.toString());
    }
  }

  setupEventListeners() {
    this.tabs_.addEventListener(
        'slotchange', () => this.initializeOrProcessTabs());

    // Add event listener for keyboard navigation and tab clicks.
    const tablist = this.getRequiredElement('#tablist');
    tablist.addEventListener('keydown', e => this.onKeydown_(e));
    tablist.addEventListener('click', (e: MouseEvent) => {
      const clickedElement =
          (e.target as HTMLElement).closest<HTMLElement>('[slot="tab"]');
      if (!clickedElement) {
        return;
      }

      const index =
          this.getSlottedTabs_().findIndex(tab => tab === clickedElement);
      if (index !== -1) {
        this.setAttribute('selected-index', index.toString());
      }
    });
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    if (name === 'selected-index') {
      const newIndex = Number(newValue);
      assert(!Number.isNaN(newIndex));

      this.getSlottedPanels_().forEach((panel: Element, index: number) => {
        panel.toggleAttribute('selected', index === newIndex);
      });

      this.getSlottedTabs_().forEach((tab: HTMLElement, index: number) => {
        const isSelected = index === newIndex;
        tab.toggleAttribute('selected', isSelected);
        // Update tabIndex for a11y
        tab.setAttribute('tabindex', isSelected ? '0' : '-1');
        // Update aria-selected attribute for a11y
        const firstSelection = !tab.hasAttribute('aria-selected');
        tab.setAttribute('aria-selected', isSelected ? 'true' : 'false');
        // Update focus, but don't override initial focus.
        if (isSelected && !firstSelection) {
          tab.focus();
        }
      });

      this.dispatchEvent(new CustomEvent(
          'selected-index-change',
          {bubbles: true, composed: true, detail: newIndex}));
    }
  }

  private getSlottedTabs_(): HTMLElement[] {
    return this.tabs_.assignedElements() as HTMLElement[];
  }

  private getSlottedPanels_(): Element[] {
    const panelsSlot: HTMLSlotElement =
        this.getRequiredElement('slot[name=panel]');
    return Array.from(panelsSlot.assignedElements());
  }

  private onKeydown_(e: KeyboardEvent) {
    let delta = 0;
    switch (e.key) {
      case 'ArrowLeft':
      case 'ArrowUp':
        delta = -1;
        break;
      case 'ArrowRight':
      case 'ArrowDown':
        delta = 1;
        break;
      default:
        break;
    }

    if (!delta) {
      return;
    }

    if (document.documentElement.dir === 'rtl') {
      delta *= -1;
    }

    const tabs = this.getSlottedTabs_();
    const tabsCount = tabs.length;
    if (tabsCount === 0) {
      return;
    }

    const newIndex =
        (Number(this.getAttribute('selected-index')) + delta + tabsCount) %
        tabsCount;

    this.setAttribute('selected-index', newIndex.toString());
    // Show focus outline since we used the keyboard.
    this.focusOutlineManager_.visible = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-frame-list': CrFrameListElement;
  }
}

customElements.define('cr-frame-list', CrFrameListElement);

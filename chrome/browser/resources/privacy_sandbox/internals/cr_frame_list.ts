// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CustomElement} from '//resources/js/custom_element.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';

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

  private tabs_: HTMLElement;
  private panels_: HTMLElement;
  private focusOutlineManager_: FocusOutlineManager;

  constructor() {
    super();

    this.tabs_ = this.getRequiredElement('#tablist');
    this.panels_ = this.getRequiredElement('#tabpanels');
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
    this.setupEventListeners();
  }

  setupEventListeners() {
    this.tabs_.addEventListener('keydown', e => this.onKeydown_(e));
    this.tabs_.addEventListener('click', (e: MouseEvent) => {
      const tabs = this.getSlottedTabs_();
      const clickedTab = (e.target as HTMLElement).closest('[slot="tab"]');

      if (!clickedTab || clickedTab.getAttribute('role') === 'heading') {
        return;
      }

      const index = tabs.findIndex(tab => tab === clickedTab);
      if (index !== -1) {
        this.setAttribute('selected-index', index.toString());
      }
    });
  }

  connectedCallback() {
    if (this.hasAttribute('selected-index')) {
      return;
    }

    const tabs = this.getSlottedTabs_();
    let initialIndex = tabs.findIndex(tab => tab.hasAttribute('selected'));

    // If no tab is pre-selected, find the first non-heading tab to select.
    if (initialIndex === -1) {
      initialIndex =
          tabs.findIndex(tab => tab.getAttribute('role') !== 'heading');
    }
    this.setAttribute(
        'selected-index', (initialIndex > -1 ? initialIndex : 0).toString());
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    assert(name === 'selected-index');
    const newIndex = Number(newValue);
    assert(!Number.isNaN(newIndex));
    this.getSlottedPanels_().forEach((panel: Element, index: number) => {
      panel.toggleAttribute('selected', index === newIndex);
    });
    this.getSlottedTabs_().forEach((tab: HTMLElement, index: number) => {
      const isSelected = index === newIndex;
      tab.toggleAttribute('selected', isSelected);

      if (tab.getAttribute('role') !== 'heading') {
        // Update tabIndex for a11y
        tab.setAttribute('tabindex', isSelected ? '0' : '-1');
        // Update aria-selected attribute for a11y
        const firstSelection = !tab.hasAttribute('aria-selected');
        tab.setAttribute('aria-selected', isSelected ? 'true' : 'false');
        // Update focus, but don't override initial focus.
        if (isSelected && !firstSelection) {
          tab.focus();
        }
      }
    });

    this.dispatchEvent(new CustomEvent(
        'selected-index-change',
        {bubbles: true, composed: true, detail: newIndex}));
  }

  private getSlottedTabs_(): HTMLElement[] {
    return Array.from(this.tabs_.querySelector('slot')!.assignedElements()) as
        HTMLElement[];
  }

  private getSlottedPanels_(): Element[] {
    const slots: HTMLSlotElement =
        this.panels_.querySelector('slot[name=panel]')!;
    return Array.from(slots.assignedElements());
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

    let newIndex =
        (Number(this.getAttribute('selected-index')) + delta + tabsCount) %
        tabsCount;

    // Skip 'heading' tabs as they are not selectable.
    for (let i = 0; i < tabsCount; i++) {
      if (tabs[newIndex]?.getAttribute('role') !== 'heading') {
        break;
      }
      newIndex = (newIndex + delta + tabsCount) % tabsCount;
    }

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

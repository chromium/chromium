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
    return ['selected-index', 'collapsed'];
  }

  private tabs_: HTMLSlotElement;
  private focusOutlineManager_: FocusOutlineManager;

  constructor() {
    super();
    this.tabs_ = this.getRequiredElement<HTMLSlotElement>('slot[name=tab]');
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
  }

  connectedCallback() {
    this.setupEventListeners();
    this.initializeOrProcessTabs();
  }

  /**
   * Sets up tabs: adds icons to headings, selects the active tab, and updates
   * visibility.
   */
  initializeOrProcessTabs() {
    const tabs = this.getSlottedTabs_();
    if (tabs.length === 0) {
      return;
    }

    const iconTemplate = this.shadowRoot!.querySelector<HTMLElement>(
        '#icon-templates > .toggle-icon');

    tabs.forEach(tab => {
      if (tab.getAttribute('role') === 'heading') {
        tab.removeAttribute('collapsed');
        if (iconTemplate && !tab.querySelector('.toggle-icon')) {
          tab.prepend(iconTemplate.cloneNode(true));
        }
      }
    });

    const foundIndex = tabs.findIndex(
        tab => tab.hasAttribute('selected') ||
            tab.getAttribute('role') !== 'heading');
    const selectedIndex = foundIndex !== -1 ? foundIndex : 0;

    if (!this.hasAttribute('selected-index')) {
      this.setAttribute('selected-index', selectedIndex.toString());
    }
    this.updateAllTabsVisibility();
  }

  /** Shows or hides tabs based on whether their parent group is collapsed. */
  updateAllTabsVisibility() {
    const tabs = this.getSlottedTabs_();
    let groupIsCollapsed = false;
    let subGroupIsCollapsed = false;

    for (const tab of tabs) {
      const isGroupHeader = tab.classList.contains('settings-category-header');
      const isSubGroupHeader = tab.classList.contains('setting-header');

      if (isGroupHeader) {
        groupIsCollapsed = tab.hasAttribute('collapsed');
        subGroupIsCollapsed = false;
        continue;
      }

      if (isSubGroupHeader) {
        tab.classList.toggle('hidden-by-group', groupIsCollapsed);
        subGroupIsCollapsed = tab.hasAttribute('collapsed');
        continue;
      }

      tab.classList.toggle(
          'hidden-by-group', groupIsCollapsed || subGroupIsCollapsed);
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

      // Clicking a heading toggles its collapsed state.
      if (clickedElement.getAttribute('role') === 'heading') {
        clickedElement.toggleAttribute('collapsed');
        this.updateAllTabsVisibility();
        return;
      }

      // Clicking a regular tab selects it.
      const index =
          this.getSlottedTabs_().findIndex(tab => tab === clickedElement);
      if (index !== -1) {
        this.setAttribute('selected-index', index.toString());
      }
    });

    this.getRequiredElement('#sidebar-visibility-button')
        .addEventListener('click', () => this.toggleSidebar_());
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
        // Non-selectable heading tabs should not get focus or ARIA attributes.
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
  }

  private toggleSidebar_() {
    this.toggleAttribute('collapsed');
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

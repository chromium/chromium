// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from ui/webui/resources/cr_elements/cr_tab_box/cr_tab_box.ts.

import {assert} from '//resources/js/assert.js';
import {CustomElement} from '//resources/js/custom_element.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';

import {getTemplate} from './cr_tab_box.html.js';

declare global {
  interface HTMLElementEventMap {
    'selected-index-change': CustomEvent<number>;
  }
}

export class CrTabBoxElement extends CustomElement {
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

    const tabs = this.$<HTMLElement>('#tablist');
    assert(tabs);
    this.tabs_ = tabs;
    this.tabs_.addEventListener('keydown', e => this.onKeydown_(e));
    this.tabs_.addEventListener('click', (e: MouseEvent) => {
      const tabs = this.getTabs_();
      for (let i = 0; i < e.composedPath().length; i++) {
        const el = e.composedPath()[i] as HTMLElement;
        const index = tabs.findIndex(tab => tab === el);
        if (index !== -1) {
          this.setAttribute('selected-index', index.toString());
          break;
        }
      }
    });

    const panels = this.$<HTMLElement>('#tabpanels');
    assert(panels);
    this.panels_ = panels;
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
  }

  connectedCallback() {
    this.setAttribute('selected-index', '0');
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    assert(name === 'selected-index');
    const newIndex = Number(newValue);
    assert(!Number.isNaN(newIndex));
    this.getPanels_().forEach((panel: Element, index: number) => {
      panel.toggleAttribute('selected', index === newIndex);
    });
    this.getTabs_().forEach((tab: HTMLElement, index: number) => {
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

  private getTabs_(): HTMLElement[] {
    return Array.from(this.tabs_.querySelector('slot')!.assignedElements()) as
        HTMLElement[];
  }

  private getPanels_(): Element[] {
    return Array.from(this.panels_.querySelector('slot')!.assignedElements());
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

    const count = this.getTabs_().length;
    const newIndex =
        (Number(this.getAttribute('selected-index')) + delta + count) % count;
    this.setAttribute('selected-index', newIndex.toString());

    // Show focus outline since we used the keyboard.
    this.focusOutlineManager_.visible = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tab-box': CrTabBoxElement;
  }
}

customElements.define('cr-tab-box', CrTabBoxElement);

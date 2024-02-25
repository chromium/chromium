// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from
// ui/webui/resources/cr_elements/cr_menu_selector/cr_menu_selector.ts

import {assert} from '//resources/js/assert.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {IronSelectableBehavior} from '//resources/polymer/v3_0/iron-selector/iron-selectable.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const CrMenuSelectorBase =
    mixinBehaviors([IronSelectableBehavior], PolymerElement) as
    {new (): PolymerElement & IronSelectableBehavior};

export class CrMenuSelector extends CrMenuSelectorBase {
  static get is() {
    return 'cr-menu-selector';
  }

  private focusOutlineManager_: FocusOutlineManager;

  override connectedCallback() {
    super.connectedCallback();
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
  }

  override ready() {
    super.ready();
    this.setAttribute('role', 'menu');
    this.addEventListener('focusin', this.onFocusin_.bind(this));
    this.addEventListener('keydown', this.onKeydown_.bind(this));
    this.addEventListener(
        'iron-deselect',
        e => this.onIronDeselected_(e as CustomEvent<{item: HTMLElement}>));
    this.addEventListener(
        'iron-select',
        e => this.onIronSelected_(e as CustomEvent<{item: HTMLElement}>));
  }

  private getAllFocusableItems_(): HTMLElement[] {
    // Note that this is different from IronSelectableBehavior's items property
    // as some items are focusable and actionable but not selectable (eg. an
    // external link).
    return Array.from(
        this.querySelectorAll('[role=menuitem]:not([disabled]):not([hidden])'));
  }

  private onFocusin_(e: FocusEvent) {
    // If the focus was moved by keyboard and is coming in from a relatedTarget
    // that is not within this menu, move the focus to the first menu item. This
    // ensures that the first menu item is always the first focused item when
    // focusing into the menu. A null relatedTarget means the focus was moved
    // from outside the WebContents.
    const focusMovedWithKeyboard = this.focusOutlineManager_.visible;
    const focusMovedFromOutside = e.relatedTarget === null ||
        !this.contains(e.relatedTarget as HTMLElement);
    if (focusMovedWithKeyboard && focusMovedFromOutside) {
      this.getAllFocusableItems_()[0]!.focus();
    }
  }

  private onIronDeselected_(e: CustomEvent<{item: HTMLElement}>) {
    e.detail.item.removeAttribute('aria-current');
  }

  private onIronSelected_(e: CustomEvent<{item: HTMLElement}>) {
    e.detail.item.setAttribute('aria-current', 'page');
  }

  private onKeydown_(event: KeyboardEvent) {
    const items = this.getAllFocusableItems_();
    assert(items.length >= 1);
    const currentFocusedIndex =
        items.indexOf(this.querySelector<HTMLElement>(':focus')!);

    let newFocusedIndex = currentFocusedIndex;
    switch (event.key) {
      case 'Tab':
        if (event.shiftKey) {
          // If pressing Shift+Tab, immediately focus the first element so that
          // when the event is finished processing, the browser automatically
          // focuses the previous focusable element outside of the menu.
          items[0]!.focus();
        } else {
          // If pressing Tab, immediately focus the last element so that when
          // the event is finished processing, the browser automatically focuses
          // the next focusable element outside of the menu.
          items[items.length - 1]!.focus({preventScroll: true});
        }
        return;
      case 'ArrowDown':
        newFocusedIndex = (currentFocusedIndex + 1) % items.length;
        break;
      case 'ArrowUp':
        newFocusedIndex =
            (currentFocusedIndex + items.length - 1) % items.length;
        break;
      case 'Home':
        newFocusedIndex = 0;
        break;
      case 'End':
        newFocusedIndex = items.length - 1;
        break;
    }

    if (newFocusedIndex === currentFocusedIndex) {
      return;
    }

    event.preventDefault();
    items[newFocusedIndex]!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-menu-selector': CrMenuSelector;
  }
}

customElements.define(CrMenuSelector.is, CrMenuSelector);

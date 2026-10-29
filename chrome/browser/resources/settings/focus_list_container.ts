// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-focus-list-container' is a container for a list of
 * items that supports arrow key navigation via HTML's focusgroup attribute and
 * restores focus to the new last item when the previously focused last item is
 * deleted. This is needed because native focusgroup does not yet support
 * this case; see https://github.com/openui/open-ui/issues/1490. Clients are
 * responsible for rendering the list items directly inside this container;
 * rendering items with a virtual or lazy list such as cr-lazy-list is not
 * supported.
 */

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './focus_list_container.css.js';
import {getHtml} from './focus_list_container.html.js';

export class SettingsFocusListContainerElement extends CrLitElement {
  static get is() {
    return 'settings-focus-list-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      focusSelector: {type: String},
    };
  }

  accessor focusSelector: string = '';

  private lastFocusedTarget_: HTMLElement|null = null;

  override connectedCallback() {
    super.connectedCallback();
    if (!this.hasAttribute('focusgroup')) {
      this.setAttribute('focusgroup', 'listbox block');
    }
    if (!this.hasAttribute('role') &&
        this.getAttribute('focusgroup')!.startsWith('listbox')) {
      this.setAttribute('role', 'listbox');
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('focusin', this.onFocusIn_);
    this.addEventListener('focusout', this.onFocusOut_);
  }

  private onFocusIn_(e: FocusEvent) {
    this.lastFocusedTarget_ = e.target as HTMLElement;
  }

  private async onFocusOut_(e: FocusEvent) {
    // If relatedTarget is non-null, focus is moving to another focusable
    // element. Clear lastFocusedTarget_ if focus is moving outside this
    // container. If focus is moving within the container, it will be updated
    // by onFocusIn_.
    if (e.relatedTarget !== null) {
      if (!this.matches(':focus-within')) {
        this.lastFocusedTarget_ = null;
      }
      return;
    }

    // If relatedTarget is null, focus was lost either because (1) the focused
    // element is being removed from the DOM, or (2) the user blurred it. This
    // code handles only case (2) - case (1) is handled by onSlotchange_. When
    // an element is removed, focusout fires synchronously before the node is
    // detached and slotchange fires, so to identify which case triggered
    // focusout defer checking isConnected until after DOM removal has completed
    // by awaiting updateComplete. If the element is still connected and focus
    // is not within this container, it was blurred, so clear
    // lastFocusedTarget_.
    await this.updateComplete;
    if (this.lastFocusedTarget_?.isConnected &&
        !this.matches(':focus-within')) {
      this.lastFocusedTarget_ = null;
    }
  }

  protected onSlotchange_(e: Event) {
    // Only restore focus if the last focused element was disconnected
    // and focus has not already moved elsewhere within this container.
    if (!this.lastFocusedTarget_ || this.lastFocusedTarget_.isConnected ||
        this.matches(':focus-within')) {
      return;
    }

    this.lastFocusedTarget_ = null;
    const slot = e.target as HTMLSlotElement;
    const assignedElements = slot.assignedElements() as HTMLElement[];
    if (assignedElements.length === 0) {
      return;
    }

    const lastElement = assignedElements.at(-1)!;
    const toFocus = this.focusSelector !== '' ?
        lastElement.querySelector<HTMLElement>(this.focusSelector) :
        lastElement;
    toFocus?.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-focus-list-container': SettingsFocusListContainerElement;
  }
}

customElements.define(
    SettingsFocusListContainerElement.is, SettingsFocusListContainerElement);

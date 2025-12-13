// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './viewer_bottom_toolbar_dropdown.css.js';
import {getHtml} from './viewer_bottom_toolbar_dropdown.html.js';

export class ViewerBottomToolbarDropdownElement extends CrLitElement {
  static get is() {
    return 'viewer-bottom-toolbar-dropdown';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      buttonTitle: {type: String},
      showDropdown_: {type: Boolean},
    };
  }

  accessor buttonTitle: string = '';
  protected accessor showDropdown_: boolean = false;

  private tracker_: EventTracker = new EventTracker();

  override disconnectedCallback() {
    this.tracker_.removeAll();
    super.disconnectedCallback();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('showDropdown_') && this.showDropdown_) {
      const menuSlot =
          this.shadowRoot.querySelector<HTMLSlotElement>('slot[name="menu"]');
      assert(menuSlot);
      const menuElements = menuSlot.assignedElements();
      if (menuElements.length > 0) {
        (menuElements[0]! as HTMLElement).focus();
      }
    }
  }

  protected toggleDropdown_(): void {
    this.showDropdown_ = !this.showDropdown_;

    if (this.showDropdown_) {
      this.tracker_.add(this, 'focusout', this.handleFocusOut_.bind(this));
    } else {
      this.tracker_.remove(this, 'focusout');
    }
  }

  // Exit out of the dropdown when focus shifts away from the dropdown menu.
  private handleFocusOut_(e: FocusEvent) {
    // toggleDropdown_() should have already removed this event handler.
    assert(this.showDropdown_);

    // Skip if the focus target is the menu.
    const nextElement = e.relatedTarget;
    if (nextElement instanceof HTMLElement && nextElement !== this &&
        this.contains(nextElement)) {
      return;
    }

    this.toggleDropdown_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-bottom-toolbar-dropdown': ViewerBottomToolbarDropdownElement;
  }
}

customElements.define(
    ViewerBottomToolbarDropdownElement.is, ViewerBottomToolbarDropdownElement);

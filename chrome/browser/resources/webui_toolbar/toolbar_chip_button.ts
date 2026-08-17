// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toolbar_chip_button.css.js';
import {getHtml} from './toolbar_chip_button.html.js';

export interface ToolbarChipButtonElement {
  $: {
    button: HTMLElement,
  };
}

/**
 * toolbar-chip-button is a pill-shaped button element. It is used for
 * toolbar buttons and location bar chips (e.g., app menu button, avatar
 * button, permission chips) that can have text, icons, or both.
 *
 * It is designed for 1:1 parity with C++ Native Views (e.g., LocationIconView):
 * - InkDrop Parity: Uses CSS ::before and ::after for independent hover
 *   and pressed transitions, matching C++ InkDrop behavior.
 * - Focus-Theft Survival: Supports is-menu-open attribute to lock the
 *   active state and prevent flicker when a native bubble steals focus.
 * - Native Interaction: Uses a native button and CSS :active for
 *   automatic accessibility, keyboard nav, and drag-to-cancel behavior.
 * - Label Animation: Supports sliding label transitions via animates-label
 *   and has-label attributes.
 *
 * Slots:
 * - prefix-icon: Icon before the text.
 * - Default slot: Text content.
 * - suffix-icon: Icon after the text.
 */
export class ToolbarChipButtonElement extends CrLitElement {
  static get is() {
    return 'toolbar-chip-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaLabel: {type: String},
      ariaDescription: {type: String},
      tooltip: {type: String},
      ariaHasPopup: {type: String},
      ariaExpanded: {type: String},
      disabled: {
        type: Boolean,
        reflect: true,
      },
      // Draws the focus ring outwards from the button boundary (2px gap, 2px
      // ring) instead of universally inset. Used by inner-Omnibox chips like
      // Page Actions.
      outsetFocusRing: {
        type: Boolean,
        reflect: true,
        attribute: 'outset-focus-ring',
      },
      animatesLabel: {
        type: Boolean,
        reflect: true,
        attribute: 'animates-label',
      },
      buttonTabIndex: {
        type: Number,
      },
    };
  }

  /** Accessibility label for the inner button. */
  override accessor ariaLabel: string = '';

  /** Accessibility description for the inner button. */
  override accessor ariaDescription: string = '';

  /** Tooltip text to show on hover. */
  accessor tooltip: string = '';

  /** Indicates that the button triggers a popup. */
  override accessor ariaHasPopup: string|null = null;

  /** Indicates whether the popup triggered by the button is expanded. */
  override accessor ariaExpanded: string|null = null;

  /** Whether the button is disabled. */
  accessor disabled: boolean = false;

  /**
   * Whether the label should animate (e.g., when the text content changes or
   * when the chip transitions between states).
   */
  accessor animatesLabel: boolean = false;
  // Draws an outward-expanding focus ring instead of an inset focus ring.
  accessor outsetFocusRing: boolean = false;
  accessor buttonTabIndex: number = -1;

  override focus() {
    this.$.button.focus();
  }

  protected onPrefixIconSlotchange_(e: Event) {
    const slot = e.target as HTMLSlotElement;
    this.toggleAttribute('has-prefix-icon', slot.assignedElements().length > 0);
  }

  protected onSuffixIconSlotchange_(e: Event) {
    const slot = e.target as HTMLSlotElement;
    this.toggleAttribute('has-suffix-icon', slot.assignedElements().length > 0);
  }
}

customElements.define(ToolbarChipButtonElement.is, ToolbarChipButtonElement);

declare global {
  interface HTMLElementTagNameMap {
    'toolbar-chip-button': ToolbarChipButtonElement;
  }
}

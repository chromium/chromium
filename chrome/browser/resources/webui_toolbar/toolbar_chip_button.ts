// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toolbar_chip_button.css.js';
import {getHtml} from './toolbar_chip_button.html.js';

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
      ariaLabel: {
        type: String,
        attribute: 'aria-label',
      },
      ariaDescription: {
        type: String,
        attribute: 'aria-description',
      },
      tooltip: {
        type: String,
        attribute: 'title',
      },
      ariaHasPopup: {
        type: String,
        attribute: 'aria-haspopup',
      },
      disabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  override accessor ariaLabel: string = '';
  override accessor ariaDescription: string = '';
  accessor tooltip: string = '';
  override accessor ariaHasPopup: string|null = null;
  accessor disabled: boolean = false;

  override focus() {
    this.shadowRoot?.querySelector<HTMLElement>('#button')?.focus();
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

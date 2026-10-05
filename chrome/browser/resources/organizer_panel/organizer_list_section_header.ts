// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section_header.css.js';
import {getHtml} from './organizer_list_section_header.html.js';

export interface OrganizerListSectionHeaderElement {
  $: {
    expandButton: CrExpandButtonElement,
  };
}

export class OrganizerListSectionHeaderElement extends CrLitElement {
  static get is() {
    return 'organizer-list-section-header';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },
      expanded: {
        type: Boolean,
        notify: true,
        reflect: true,
      },
    };
  }

  accessor disabled: boolean = false;
  accessor expanded: boolean = false;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    // Clicking the header label focuses the inner icon button via script, which
    // still triggers :focus-visible; track keyboard vs mouse input so CSS can
    // hide the focus ring on mouse clicks.
    FocusOutlineManager.forDocument(document);
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-header': OrganizerListSectionHeaderElement;
  }
}

customElements.define(
    OrganizerListSectionHeaderElement.is, OrganizerListSectionHeaderElement);

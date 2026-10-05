// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {MouseHoverableMixinLit} from '//resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section_header.css.js';
import {getHtml} from './organizer_list_section_header.html.js';

export interface OrganizerListSectionHeaderElement {
  $: {
    expandButton: CrExpandButtonElement,
    menu: CrActionMenuElement,
    menuButton: CrIconButtonElement,
    showAllButton: HTMLButtonElement,
    showSomeButton: HTMLButtonElement,
  };
}

const OrganizerListSectionHeaderElementBase =
    MouseHoverableMixinLit(CrLitElement);

export class OrganizerListSectionHeaderElement extends
    OrganizerListSectionHeaderElementBase {
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

  protected onMenuButtonClick_(e: Event) {
    e.stopPropagation();
    this.$.menu.showAt(this.$.menuButton, {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  protected onShowSomeClick_() {
    this.$.menu.close();
  }

  protected onShowAllClick_() {
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-header': OrganizerListSectionHeaderElement;
  }
}

customElements.define(
    OrganizerListSectionHeaderElement.is, OrganizerListSectionHeaderElement);

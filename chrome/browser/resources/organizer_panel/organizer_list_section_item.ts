// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrUrlListItemElement} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {MouseHoverableMixinLit} from '//resources/cr_elements/mouse_hoverable_mixin_lit.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section_item.css.js';
import {getHtml} from './organizer_list_section_item.html.js';

// Icon for an organizer list section item. Only one of these fields should be
// defined.
export interface OrganizerListSectionItemIcon {
  // URLs to display favicons for.
  urls?: string[];

  // Custom element to render as the icon (e.g., a tab group dot).
  element?: TemplateResult;
}

// Action button displayed when an organizer list section item is hovered.
export interface OrganizerListSectionItemActionButton {
  // Icon to display on the action button (e.g. 'cr:close').
  icon: string;

  // Accessibility label.
  ariaLabel: string;
}

// Model for a single item in an organizer list section.
export interface OrganizerListSectionItem<T> {
  // Title (main line) of the item.
  title: string;

  // Description (secondary line) of the item.
  description?: string[];

  // Icon displayed at the beginning of the item.
  prefixIcon?: OrganizerListSectionItemIcon;

  // Optional: Icon to be displayed at the end of the item.
  trailingIcon?: string;

  // Optional: If defined, replaces the |trailingIcon| with a <cr-icon-button>
  // when the item is hovered. Clicking on the button will call
  // onItemActionButtonClicked on the delegate.
  hoveredActionButton?: OrganizerListSectionItemActionButton;

  // The actual data held by the item.
  data?: T;
}

export interface OrganizerListSectionItemElement {
  $: {
    actionButton: CrIconButtonElement,
    crUrlListItem: CrUrlListItemElement,
    trailingIcon: CrIconElement,
  };
}

const OrganizerListSectionItemElementBase =
    MouseHoverableMixinLit(CrLitElement);

export class OrganizerListSectionItemElement extends
    OrganizerListSectionItemElementBase {
  static get is() {
    return 'organizer-list-section-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      item: {type: Object},
    };
  }

  accessor item: OrganizerListSectionItem<unknown> = {
    title: '',
  };

  protected getDescription_(): string {
    return this.item.description?.join(' · ') || '';
  }

  protected getUrl_(): string|undefined {
    // TODO(b/549786784): Support multiple URLs for stacked favicons.
    return this.item.prefixIcon?.urls?.[0];
  }

  protected hasSuffix_(): boolean {
    return !!(this.item.trailingIcon || this.item.hoveredActionButton);
  }

  protected hasActionButton_(): boolean {
    return !!this.item.hoveredActionButton;
  }

  protected onActionButtonClick_(e: Event) {
    e.stopPropagation();
    this.fire('action-button-click', {
      item: this.item,
      buttonElement: e.currentTarget as HTMLElement,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-item': OrganizerListSectionItemElement;
  }
}

customElements.define(
    OrganizerListSectionItemElement.is, OrganizerListSectionItemElement);

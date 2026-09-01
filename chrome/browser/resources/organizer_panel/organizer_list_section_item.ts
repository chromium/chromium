// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

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

// Model for a single item in an organizer list section.
export interface OrganizerListSectionItem {
  // Title (main line) of the item.
  title: string;

  // Description (secondary line) of the item.
  description?: string[];

  // Icon displayed at the beginning of the item.
  prefixIcon?: OrganizerListSectionItemIcon;
}

export interface OrganizerListSectionItemElement {
  $: {
    crUrlListItem: CrUrlListItemElement,
  };
}

export class OrganizerListSectionItemElement extends CrLitElement {
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

  accessor item: OrganizerListSectionItem = {
    title: '',
  };

  protected getDescription_(): string {
    return this.item.description?.join(' · ') || '';
  }

  protected getUrl_(): string|undefined {
    // TODO(b/549786784): Support multiple URLs for stacked favicons.
    return this.item.prefixIcon?.urls?.[0];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-item': OrganizerListSectionItemElement;
  }
}

customElements.define(
    OrganizerListSectionItemElement.is, OrganizerListSectionItemElement);

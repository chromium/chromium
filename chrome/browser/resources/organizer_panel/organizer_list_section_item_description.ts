// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section_item_description.css.js';
import {getHtml} from './organizer_list_section_item_description.html.js';

// Single description segment for an organizer list section item.
export interface OrganizerListSectionItemDescriptionPart {
  // Text of the description.
  text: string;

  // Optional: Whether the text should be ellided from the start of the string
  // instead of the end.
  elideFromStart?: boolean;

  // Optional: Element shown before the text part.
  prefixElement?: TemplateResult;
}

export interface OrganizerListSectionItemDescriptionElement {
  $: {
    descriptionParts: HTMLElement,
  };
}

export class OrganizerListSectionItemDescriptionElement extends CrLitElement {
  static get is() {
    return 'organizer-list-section-item-description';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      descriptionParts: {type: Array},
    };
  }

  accessor descriptionParts: OrganizerListSectionItemDescriptionPart[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-item-description':
        OrganizerListSectionItemDescriptionElement;
  }
}

customElements.define(
    OrganizerListSectionItemDescriptionElement.is,
    OrganizerListSectionItemDescriptionElement);

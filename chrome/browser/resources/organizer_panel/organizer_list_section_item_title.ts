// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import type {Range} from '/tab_group_shared/search.js';

import {getCss} from './organizer_list_section_item_title.css.js';
import {getHtml} from './organizer_list_section_item_title.html.js';
import {renderHighlightedText} from './search_utils.js';

export interface OrganizerListSectionItemTitleElement {
  $: {
    titleParts: HTMLElement,
  };
}

export class OrganizerListSectionItemTitleElement extends CrLitElement {
  static get is() {
    return 'organizer-list-section-item-title';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      titleParts: {type: Array},
      highlightRanges: {type: Array},
    };
  }

  accessor titleParts: string[] = [];
  accessor highlightRanges: Range[][] = [];

  protected renderTitlePart_(text: string, index: number): TemplateResult
      |string {
    return renderHighlightedText(text, this.highlightRanges[index]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section-item-title': OrganizerListSectionItemTitleElement;
  }
}

customElements.define(
    OrganizerListSectionItemTitleElement.is,
    OrganizerListSectionItemTitleElement);

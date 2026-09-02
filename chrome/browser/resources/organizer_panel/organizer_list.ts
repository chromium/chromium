// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './organizer_list_section.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list.css.js';
import {getHtml} from './organizer_list.html.js';
import type {OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';

export interface OrganizerListElement {
  $: {
    sections: HTMLElement,
  };
}

export class OrganizerListElement extends CrLitElement {
  static get is() {
    return 'organizer-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      sectionDelegates: {type: Array},
      searchQuery: {type: String},
    };
  }

  accessor sectionDelegates: Array<OrganizerListSectionDelegate<unknown>> = [];
  accessor searchQuery: string = '';
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list': OrganizerListElement;
  }
}

customElements.define(OrganizerListElement.is, OrganizerListElement);

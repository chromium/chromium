// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './organizer_list_section_item.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section.css.js';
import {getHtml} from './organizer_list_section.html.js';
import type {OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';

export class OrganizerListSectionElement extends CrLitElement {
  static get is() {
    return 'organizer-list-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
    };
  }

  accessor delegate: OrganizerListSectionDelegate|null = null;
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section': OrganizerListSectionElement;
  }
}

customElements.define(
    OrganizerListSectionElement.is, OrganizerListSectionElement);

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './organizer_list_section.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list.css.js';
import {getHtml} from './organizer_list.html.js';
import type {OrganizerListSectionElement} from './organizer_list_section.js';
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
      allSectionsHidden_: {type: Boolean},
    };
  }

  accessor sectionDelegates: Array<OrganizerListSectionDelegate<unknown>> = [];
  accessor searchQuery: string = '';
  protected accessor allSectionsHidden_: boolean = false;

  private getSections_(): OrganizerListSectionElement[] {
    return Array.from(
        this.shadowRoot.querySelectorAll('organizer-list-section'));
  }

  protected onSectionHiddenChanged_() {
    const sections = this.getSections_();
    this.allSectionsHidden_ =
        sections.length > 0 && sections.every(s => s.hidden);
  }

  areAllSectionsExpanded(): boolean {
    const sections = this.getSections_();
    return sections.length > 0 && sections.every(s => s.expanded);
  }

  expandAll() {
    for (const section of this.getSections_()) {
      section.setExpanded(true);
    }
  }

  collapseAll() {
    for (const section of this.getSections_()) {
      section.setExpanded(false);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list': OrganizerListElement;
  }
}

customElements.define(OrganizerListElement.is, OrganizerListElement);

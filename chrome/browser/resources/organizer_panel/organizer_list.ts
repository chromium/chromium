// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './organizer_list_section.js';
import './organizer_zero_state.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {KeyArrowNavigationService} from './keyboard_arrow_navigation_service.js';
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

  private keyArrowNavigationService_: KeyArrowNavigationService;
  private boundFocusNextSection_ = this.onFocusNextSection_.bind(this);
  private boundFocusPreviousSection_ = this.onFocusPreviousSection_.bind(this);
  private boundFocusCurrentSection_ = this.onFocusCurrentSection_.bind(this);

  constructor() {
    super();
    this.keyArrowNavigationService_ =
        new KeyArrowNavigationService(this, () => this.getNavigableElements());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.keyArrowNavigationService_.startListening();
    this.addEventListener('focus-next-section', this.boundFocusNextSection_);
    this.addEventListener(
        'focus-previous-section', this.boundFocusPreviousSection_);
    this.addEventListener(
        'focus-current-section', this.boundFocusCurrentSection_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.keyArrowNavigationService_.stopListening();
    this.removeEventListener('focus-next-section', this.boundFocusNextSection_);
    this.removeEventListener(
        'focus-previous-section', this.boundFocusPreviousSection_);
    this.removeEventListener(
        'focus-current-section', this.boundFocusCurrentSection_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('sectionDelegates') ||
        changedProperties.has('searchQuery')) {
      this.keyArrowNavigationService_.rebuildNavigationElements();
    }
  }

  private getSections_(): OrganizerListSectionElement[] {
    return Array.from(
        this.shadowRoot.querySelectorAll('organizer-list-section'));
  }

  getVisibleSections(): OrganizerListSectionElement[] {
    return this.getSections_().filter(s => !s.hidden);
  }

  getNavigableElements(): HTMLElement[] {
    return this.getVisibleSections().flatMap(s => s.getNavigableElements());
  }

  focusFirstSection(): boolean {
    const sections = this.getVisibleSections();
    if (sections.length <= 0) {
      return false;
    }
    sections[0]!.focus();
    return true;
  }

  focusLastSection(focusMenuButtonIfVisible: boolean = false): boolean {
    const sections = this.getVisibleSections();
    if (sections.length <= 0) {
      return false;
    }

    const lastSection = sections[sections.length - 1]!;
    if (focusMenuButtonIfVisible &&
        lastSection.$.header.isMenuButtonVisible()) {
      lastSection.$.header.focusMenuButton();
    } else {
      lastSection.focus();
    }
    return true;
  }

  protected onSectionHiddenChanged_() {
    const sections = this.getSections_();
    this.allSectionsHidden_ =
        sections.length > 0 && sections.every(s => s.hidden);
    this.keyArrowNavigationService_.rebuildNavigationElements();
  }

  protected onSectionExpandedChanged_() {
    this.keyArrowNavigationService_.rebuildNavigationElements();
  }

  private onFocusNextSection_(e: Event) {
    const customEvent = e as CustomEvent<{sourceEvent?: KeyboardEvent}>;
    const sections = this.getVisibleSections();
    const path = e.composedPath();
    const currentSection = sections.find(s => path.includes(s));
    const currentIndex = currentSection ? sections.indexOf(currentSection) : -1;

    if (currentIndex === -1 || currentIndex === sections.length - 1) {
      this.fire('focus-after-list', customEvent.detail);
    } else {
      sections[currentIndex + 1]!.focus();
      customEvent.detail?.sourceEvent?.preventDefault();
      customEvent.detail?.sourceEvent?.stopPropagation();
    }
  }

  private onFocusPreviousSection_(e: Event) {
    const customEvent = e as CustomEvent<{sourceEvent?: KeyboardEvent}>;
    const sections = this.getVisibleSections();
    const path = e.composedPath();
    const currentSection = sections.find(s => path.includes(s));
    const currentIndex = currentSection ? sections.indexOf(currentSection) : -1;

    if (currentIndex <= 0) {
      this.fire('focus-before-list', customEvent.detail);
    } else {
      const prevSection = sections[currentIndex - 1]!;
      if (prevSection.$.header.isMenuButtonVisible()) {
        prevSection.$.header.focusMenuButton();
      } else {
        prevSection.focus();
      }
      customEvent.detail?.sourceEvent?.preventDefault();
      customEvent.detail?.sourceEvent?.stopPropagation();
    }
  }

  private onFocusCurrentSection_(e: Event) {
    const customEvent = e as CustomEvent<{sourceEvent?: KeyboardEvent}>;
    const sections = this.getVisibleSections();
    const path = e.composedPath();
    const currentSection = sections.find(s => path.includes(s));
    if (currentSection) {
      currentSection.focus();
      customEvent.detail?.sourceEvent?.preventDefault();
      customEvent.detail?.sourceEvent?.stopPropagation();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list': OrganizerListElement;
  }
}

customElements.define(OrganizerListElement.is, OrganizerListElement);

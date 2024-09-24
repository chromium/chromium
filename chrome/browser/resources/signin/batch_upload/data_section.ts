// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {DataContainer} from './batch_upload.js';
import {getCss} from './data_section.css.js';
import {getHtml} from './data_section.html.js';

// Used for initialization only.
function createEmptyContainer(): DataContainer {
  return {
    sectionTitle: '',
    dialogSubtitle: '',
    dataItems: [],
  };
}

export interface DataSectionElement {
  $: {
    collapse: CrCollapseElement,
  };
}

export class DataSectionElement extends CrLitElement {
  static get is() {
    return 'data-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataContainer: {type: Object},
      expanded_: {type: Boolean},
      dataSelectedCount_: {type: Number},
    };
  }

  // Data to be displayed.
  dataContainer: DataContainer = createEmptyContainer();
  // If the collapse section is exapnded.
  protected expanded_: boolean = false;

  // Map containing the ids of the selected items in the section. Initialized
  // with all the ids of the section.
  // To be used as the output of the section as well for the parent element.
  dataSelected: Set<number> = new Set<number>();
  protected dataSelectedCount_: number = 0;

  override connectedCallback() {
    super.connectedCallback();

    this.initializeSection_();

    this.requestUpdate();
  }

  // Initializes the output variable based on the input.
  // Expected to be called once.
  private initializeSection_() {
    // And any section should not be empty.
    assert(
        this.dataContainer.dataItems !== undefined &&
            this.dataContainer.dataItems.length !== 0,
        'Sections should have at least one item to show.');

    this.dataContainer.dataItems.forEach((item) => {
      // Ids within a section should not be repeated.
      assert(
          !this.dataSelected.has(item.id),
          item.id + ' already exists in this section.' +
              ' An Id should be unique per section');
      this.dataSelected.add(item.id);
    });

    this.dataSelectedCount_ = this.dataSelected.size;
  }

  protected getSectionTitle_(): string {
    return this.dataContainer.sectionTitle + ' (' + this.dataSelectedCount_ +
        ')';
  }

  protected onExpandClicked_(e: Event) {
    const currentTarget = e.currentTarget as CrExpandButtonElement;
    // Opposite to make sure the icon matches the expansion.
    this.expanded_ = !currentTarget.expanded;

    // Listen to the collapse transition end to properly update the container
    // height.
    // TODO(b/363205568): this is currently not smooth; potentially listening to
    // several updates, or computing the final height and triggering it
    // immediately.
    const updateViewHeight = (e: Event) => {
      if (e.composedPath()[0] === this.$.collapse) {
        this.$.collapse.removeEventListener('transitionend', updateViewHeight);
        // Request parent container to update its height.
        this.dispatchEvent(
            new CustomEvent('update-view-height', {composed: true}));
      }
    };
    this.$.collapse.addEventListener('transitionend', updateViewHeight);
  }

  protected onCheckedChanged_(e: CustomEvent<boolean>) {
    const currentTarget = e.currentTarget as HTMLElement;
    const itemId = Number(currentTarget.dataset['id']);

    // Add or remove the items from the output set based on the checkbox value.
    if (e.detail) {
      this.dataSelected.add(itemId);
    } else {
      this.dataSelected.delete(itemId);
    }

    // Triggers update of the section title.
    this.dataSelectedCount_ = this.dataSelected.size;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-section': DataSectionElement;
  }
}

customElements.define(DataSectionElement.is, DataSectionElement);

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
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {DataContainer} from './batch_upload.js';
import {getCss} from './data_section.css.js';
import {getHtml} from './data_section.html.js';

// Used for initialization only.
function createEmptyContainer(): DataContainer {
  return {
    sectionTitle: '',
    dataItems: [],
  };
}

// Update request count, to be used along the transition duration to compute the
// interval time requests.
const UPDATE_REQUEST_COUNT: number = 10;

export interface DataSectionElement {
  $: {
    sectionTitle: HTMLElement,
    expandButton: CrExpandButtonElement,
    separator: HTMLElement,
    toggle: CrToggleElement,
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
      disabled_: {type: Boolean},
      dataSelectedCount_: {type: Number},
    };
  }

  // Data to be displayed.
  dataContainer: DataContainer = createEmptyContainer();

  // If the collapse section is exapnded.
  protected expanded_: boolean = false;
  // If the section toggle is off.
  protected disabled_: boolean = false;

  // Map containing the ids of the selected items in the section. Initialized
  // with all the ids of the section.
  // To be used as the output of the section as well for the parent element.
  dataSelected: Set<number> = new Set<number>();
  protected dataSelectedCount_: number = 0;

  // Animation variables used to update the main view height based on the
  // collapse animation duration. Initialized to 0 and gets their values in
  // `firstUpdated()` which are not expected to be modified later.
  private intervalDurationOfUpdateHeightRequests_: number = 0;
  private collapseAnimationDuration_: number = 0;

  override connectedCallback() {
    super.connectedCallback();

    this.initializeSectionOutput_();
  }

  override firstUpdated() {
    // Compute the animation duration/intervals once on startup.
    this.collapseAnimationDuration_ = parseInt(
        this.style.getPropertyValue('--iron-collapse-transition-duration'));
    this.intervalDurationOfUpdateHeightRequests_ =
        this.collapseAnimationDuration_ / UPDATE_REQUEST_COUNT;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    // Cast necessary since `expanded_` is protected.
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('expanded_')) {
      setTimeout(() => {
        this.updateViewHeightInterval_(
            this.intervalDurationOfUpdateHeightRequests_);
      }, this.intervalDurationOfUpdateHeightRequests_);
    }
  }

  // Initializes the output variable based on the input.
  private initializeSectionOutput_() {
    this.dataSelected.clear();

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

  // Resets the element to the default based on the disabled state.
  private resetWithState_(disabled: boolean) {
    if (disabled) {
      this.dataSelected.clear();
      this.dataSelectedCount_ = 0;
    } else {
      this.initializeSectionOutput_();
    }

    this.expanded_ = false;
    this.disabled_ = disabled;
  }

  // Secondary part of the title as '(N)' with N being the number of item
  // selected if greater than 0; otherwise return an empty string.
  private getSectionTitleExtraInfo_() {
    if (this.dataSelectedCount_ === 0) {
      return '';
    }

    return ' (' + this.dataSelectedCount_ + ')';
  }

  protected getSectionTitle_(): string {
    return this.dataContainer.sectionTitle + this.getSectionTitleExtraInfo_();
  }

  // Fire repetitive updates to the parent view height separated by the computed
  // interval, until the animation duration elapsed.
  private updateViewHeightInterval_(timeElapsed: number) {
    this.fire('update-view-height');
    // Animation time elapsed, animation should match the collapse animation. No
    // more view updates needed.
    if (timeElapsed >= this.collapseAnimationDuration_) {
      return;
    }

    // Trigger next update interval with the updated elapsed time.
    setTimeout(() => {
      this.updateViewHeightInterval_(
          timeElapsed + this.intervalDurationOfUpdateHeightRequests_);
    }, this.intervalDurationOfUpdateHeightRequests_);
  }

  // Needs to react to both property change (through a reset) and user action.
  protected onExpandChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  // Needs to react to both property change (through a reset caused from all
  // checkboxes being unselected) and user action.
  protected onToggleChanged_(e: CustomEvent<{value: boolean}>) {
    this.resetWithState_(/*disabled=*/ !e.detail.value);

    // Notify the parent with the new toggle value.
    this.fire('toggle-changed', {toggle: e.detail.value});
  }

  protected isCheckboxChecked_(itemId: number): boolean {
    return this.dataSelected.has(itemId);
  }

  protected onCheckedChanged_(e: CustomEvent<boolean>) {
    const currentTarget = e.currentTarget as HTMLElement;
    const itemId = Number(currentTarget.dataset['id']);

    // Checkbox on.
    if (e.detail) {
      this.dataSelected.add(itemId);
      // Triggers update of the section title.
      this.dataSelectedCount_ = this.dataSelected.size;
      return;
    }

    // Checkbox off.
    this.dataSelected.delete(itemId);
    this.dataSelectedCount_ = this.dataSelected.size;
    // If this is the last item unchecked then disable and reset the section.
    if (this.dataSelectedCount_ === 0) {
      this.resetWithState_(/*disabled=*/ true);
    }
  }

  protected isStrEmpty_(str: string) {
    return (!str || str.length === 0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-section': DataSectionElement;
  }
}

customElements.define(DataSectionElement.is, DataSectionElement);

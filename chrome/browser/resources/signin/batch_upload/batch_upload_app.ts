// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';

import type {DataContainer} from './batch_upload.js';
import {getCss} from './batch_upload_app.css.js';
import {getHtml} from './batch_upload_app.html.js';
import {BatchUploadBrowserProxyImpl} from './browser_proxy.js';
import type {BatchUploadBrowserProxy} from './browser_proxy.js';

export interface BatchUploadAppElement {
  $: {
    batchUploadDialog: HTMLElement,
  };
}

const BatchUploadAppElementBase = I18nMixinLit(CrLitElement);

export class BatchUploadAppElement extends BatchUploadAppElementBase {
  static get is() {
    return 'batch-upload-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataSections_: {type: Array},
      dataSectionsExpanded_: {type: Array},
    };
  }

  private batchUploadBrowserProxy_: BatchUploadBrowserProxy =
      BatchUploadBrowserProxyImpl.getInstance();

  // Stores the input coming from the browser initialized in
  // `initializeInputAndOutputData_()`.
  protected dataSections_: DataContainer[] = [];
  // Mapping between the collapse section and the expand button indexed by the
  // input sections
  protected dataSectionsExpanded_: boolean[] = [];

  // To be used as the output in `saveToAccount_()`. The size maps directly to
  // the input `dataSections_`. It is initialized in
  // `initializeInputAndOutputData_()`.
  private dataSectionsSelected_: Array<Array<[boolean, number]>> = [];

  override connectedCallback() {
    super.connectedCallback();

    this.batchUploadBrowserProxy_.callbackRouter.sendDataItems.addListener(
        (containerList: DataContainer[]) => {
          // Populate data with input from browser.
          this.initializeInputAndOutputData_(containerList);

          this.requestUpdate();

          this.updateViewHeight_();
        });
  }

  // Requests the browser to update the native view to match the current height
  // of the web view.
  private async updateViewHeight_() {
    await this.updateComplete;

    // TODO(b/363207887): Fix initial height.
    const height = this.$.batchUploadDialog.clientHeight;
    this.batchUploadBrowserProxy_.handler.updateViewHeight(height);
  }

  // Creates and validates the input structure that the Ui assumes for display.
  // Creates the equivalent output variable of selected ids.
  // Expected to be called once.
  private initializeInputAndOutputData_(containerList: DataContainer[]) {
    assert(this.dataSections_.length === 0);
    assert(this.dataSectionsExpanded_.length === 0);
    assert(this.dataSectionsSelected_.length === 0);

    // A data container is equivalent to a section in the Ui.
    this.dataSections_ = containerList;
    this.dataSectionsExpanded_ = Array(this.dataSections_.length).fill(false);

    // There should be at least one section.
    assert(
        this.dataSections_ !== undefined && this.dataSections_.length !== 0,
        'There should at least be one section to show.');

    for (const section of this.dataSections_) {
      // And any section should not be empty.
      assert(
          section.dataItems !== undefined && section.dataItems.length !== 0,
          'Sections should have at least one item to show.');

      const sectionSelected: Array<[boolean, number]> = [];
      const sectionItemsIdSet = new Set<number>();
      for (const item of section.dataItems) {
        // Ids within a section should not be repeated.
        assert(
            !sectionItemsIdSet.has(item.id),
            item.id + ' already exists in this section.' +
                ' An Id should be unique per section');
        sectionItemsIdSet.add(item.id);

        // By default all items are selected at the start.
        sectionSelected.push([true, item.id]);
      }

      this.dataSectionsSelected_.push(sectionSelected);
    }
  }

  protected close_() {
    this.batchUploadBrowserProxy_.handler.close();
  }

  protected saveToAccount_() {
    const idsToMove: number[][] = [];
    // Read the selected output value to extract the ids.
    for (const sectionSelected of this.dataSectionsSelected_) {
      const idsToMoveSection: number[] = [];
      for (const selectedItem of sectionSelected) {
        // Only add the item id if it is selected.
        if (selectedItem[0]) {
          idsToMoveSection.push(selectedItem[1]);
        }
      }
      idsToMove.push(idsToMoveSection);
    }

    this.batchUploadBrowserProxy_.handler.saveToAccount(idsToMove);
  }

  protected getDialogSubtitle_(): string {
    // Dialog may start loading before receiving the data.
    if (!this.dataSections_ || this.dataSections_.length === 0) {
      return '';
    }

    return this.dataSections_[0]!.dialogSubtitle;
  }

  protected getSectionTitle_(sectionIndex: number): string {
    const sectionSelected = this.dataSectionsSelected_[sectionIndex]!;
    // Count the number of selected items in this section.
    const selectedItemCount = sectionSelected.reduce((sum, [selected]) => {
      return sum + (selected ? 1 : 0);
    }, 0);

    return this.dataSections_[sectionIndex]!.sectionTitle + ' (' +
        selectedItemCount + ')';
  }

  protected onExpandClicked_(e: Event) {
    const currentTarget = e.currentTarget as CrExpandButtonElement;

    // Getting the index set in `data-index` property.
    const index = Number(currentTarget.dataset['index']);

    // Opposite to make sure the icon matches the expansion.
    this.dataSectionsExpanded_[index] = !currentTarget.expanded;

    // Listen to the collapse transition end to properly update the view height.
    // TODO(b/363205568): this is currently not smooth; potentially listening to
    // several updates, or computing the final height and triggering it
    // immediately.
    const colapseElement = this.shadowRoot!.querySelector<CrCollapseElement>(
        `cr-collapse[data-index="${index}"]`)!;
    const updateViewHeight = (e: Event) => {
      if (e.composedPath()[0] === colapseElement) {
        colapseElement.removeEventListener('transitionend', updateViewHeight);
        this.updateViewHeight_();
      }
    };
    colapseElement.addEventListener('transitionend', updateViewHeight);

    // This is needed because Lit is not aware of subproperty elements changes
    // (elements in `this.dataSectionsExpanded_` in this case). So we trigger it
    // manually.
    this.requestUpdate();
  }

  protected onCheckedChanged_(e: CustomEvent<boolean>) {
    const currentTarget = e.currentTarget as HTMLElement;

    const sectionIndex = Number(currentTarget.dataset['sectionIndex']);
    const itemIndex = Number(currentTarget.dataset['itemIndex']);
    this.dataSectionsSelected_[sectionIndex]![itemIndex]![0] = e.detail;

    // Used to update the section title through `getSectionTitle_` with the
    // number of selected items.
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'batch-upload-app': BatchUploadAppElement;
  }
}

customElements.define(BatchUploadAppElement.is, BatchUploadAppElement);

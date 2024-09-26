// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './data_section.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
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
    saveButton: CrButtonElement,
    cancelButton: CrButtonElement,
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
      isSaveEnabled_: {type: Boolean},
    };
  }

  private batchUploadBrowserProxy_: BatchUploadBrowserProxy =
      BatchUploadBrowserProxyImpl.getInstance();

  // Stores the input coming from the browser initialized in
  // `initializeInputAndOutputData_()`.
  protected dataSections_: DataContainer[] = [];

  // State of the section toggles, this is needed to control the save button
  // state.
  protected dataSectionsToggles_: boolean[] = [];

  // Whether save to account button is enabled or not.
  protected isSaveEnabled_: boolean = true;

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

  // Request the browser to update the native view to match the current height
  // of the web view.
  protected async updateViewHeight_() {
    await this.updateComplete;

    // TODO(b/363207887): Fix initial height.
    const height = this.$.batchUploadDialog.clientHeight;
    this.batchUploadBrowserProxy_.handler.updateViewHeight(height);
  }

  protected onSectionToggleChanged_(e: Event) {
    const customEvent = e as CustomEvent;
    const sectionIndex = Number((e.target as HTMLElement).dataset['index']);
    this.dataSectionsToggles_[sectionIndex] = customEvent.detail.toggle;
    this.updateSaveEnabled_();
  }

  // Initializes the input structure that the Ui uses for display.
  // Expected to be called once.
  private initializeInputAndOutputData_(containerList: DataContainer[]) {
    assert(this.dataSections_.length === 0);

    // A data container is equivalent to a section in the Ui.
    this.dataSections_ = containerList;

    // Sections are enabled by default.
    this.dataSectionsToggles_ = Array(this.dataSections_.length).fill(true);

    // There should be at least one section.
    assert(
        this.dataSections_ !== undefined && this.dataSections_.length !== 0,
        'There should at least be one section to show.');
  }

  protected close_() {
    this.batchUploadBrowserProxy_.handler.close();
  }

  private updateSaveEnabled_() {
    // If at least one section is not disabled, then save is allowed.
    this.isSaveEnabled_ =
        this.dataSectionsToggles_.some((value: boolean) => value);
  }

  protected saveToAccount_() {
    assert(this.isSaveEnabled_);

    const idsToMove: number[][] = [];

    // Get the section element list.
    const dataSections = this.shadowRoot!.querySelectorAll(`data-section`);
    // Getting the output from each section.
    for (let i = 0; i < dataSections.length; ++i) {
      const selectedIds = dataSections[i]!.dataSelected;
      idsToMove.push([...selectedIds]);
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
}

declare global {
  interface HTMLElementTagNameMap {
    'batch-upload-app': BatchUploadAppElement;
  }
}

customElements.define(BatchUploadAppElement.is, BatchUploadAppElement);

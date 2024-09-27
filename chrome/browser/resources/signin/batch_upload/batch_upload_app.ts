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

import type {BatchUploadAccountInfo, BatchUploadData, DataContainer} from './batch_upload.js';
import {getCss} from './batch_upload_app.css.js';
import {getHtml} from './batch_upload_app.html.js';
import {BatchUploadBrowserProxyImpl} from './browser_proxy.js';
import type {BatchUploadBrowserProxy} from './browser_proxy.js';

function createEmptyAccountInfo() {
  return {
    email: '',
    dataPictureUrl: '',
  };
}

export interface BatchUploadAppElement {
  $: {
    batchUploadDialog: HTMLElement,
    dataContainer: HTMLElement,
    dataSections: HTMLElement,
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
      accountInfo_: {type: Object},
      dialogSubtitle_: {type: String},
      dataSections_: {type: Array},
      isSaveEnabled_: {type: Boolean},
    };
  }

  private batchUploadBrowserProxy_: BatchUploadBrowserProxy =
      BatchUploadBrowserProxyImpl.getInstance();

  // Account information displayed in the view.
  protected accountInfo_: BatchUploadAccountInfo = createEmptyAccountInfo();
  protected dialogSubtitle_: string = '';

  // Stores the input coming from the browser initialized in
  // `initializeInputAndOutputData_()`.
  protected dataSections_: DataContainer[] = [];

  // State of the section toggles, this is needed to control the save button
  // state.
  protected dataSectionsToggles_: boolean[] = [];

  // Whether save to account button is enabled or not.
  protected isSaveEnabled_: boolean = true;

  // Observes the size of `dataSections` to know whether to show a border or not
  // if scrolling is possible.
  private resizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addResizeObserver_();

    this.batchUploadBrowserProxy_.callbackRouter.sendBatchUploadData
        .addListener((batchUploadData: BatchUploadData) => {
          this.accountInfo_ = batchUploadData.accountInfo;
          this.dialogSubtitle_ = batchUploadData.dialogSubtitle;

          // Populate data with input from browser.
          this.initializeInputAndOutputData_(batchUploadData.dataContainers);

          this.requestUpdate();

          this.updateViewHeight_();
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_!.disconnect();
  }

  private addResizeObserver_() {
    const dataContainer = this.$.dataContainer;
    this.resizeObserver_ = new ResizeObserver(() => {
      const scrollbarVisible =
          dataContainer.scrollHeight > dataContainer.clientHeight;
      // Show the container border line if the scroll bar is visible.
      dataContainer.classList.toggle('border-line', scrollbarVisible);
      // Adapt the section padding if the scrollbar is visible by overriding the
      // value (removing the scrollbar width).
      this.$.dataSections.classList.toggle(
          'data-sections-with-scrollbar', scrollbarVisible);
    });
    this.resizeObserver_.observe(dataContainer);
  }

  // Request the browser to update the native view to match the current height
  // of the web view.
  protected async updateViewHeight_() {
    await this.updateComplete;

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
}

declare global {
  interface HTMLElementTagNameMap {
    'batch-upload-app': BatchUploadAppElement;
  }
}

customElements.define(BatchUploadAppElement.is, BatchUploadAppElement);

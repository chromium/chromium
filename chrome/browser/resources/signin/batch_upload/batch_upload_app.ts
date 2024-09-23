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

import {getCss} from './batch_upload_app.css.js';
import {getHtml} from './batch_upload_app.html.js';
import {BatchUploadBrowserProxyImpl} from './browser_proxy.js';
import type {BatchUploadBrowserProxy} from './browser_proxy.js';

// TODO(b/363204445): to be removed with real implementation.
// These structures are temporary and will be replaced with mojo generated
// structures to be used and filled by the browser side.
class Data {
  id: number = -1;
  iconUrl: string = '';
  title: string = '';
  subtitle: string = '';
}

class DataSection {
  sectionTitle: string = '';
  sectionSubtitle: string = '';
  dataItems: Data[] = [];
}

enum DataType {
  PASSWORDS,
  ADDRESSES,
}

// TODO(b/363204445): to be removed with real implementation. Used for temporary
// manual testing only.
function createDummyData(dataType: DataType): DataSection {
  if (dataType === DataType.PASSWORDS) {
    const dataItem = new Data();
    dataItem.id = 1;
    dataItem.iconUrl = 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
    dataItem.title = 'password1';
    dataItem.subtitle = 'username1';
    const dataItem2 = new Data();
    dataItem2.id = 2;
    dataItem2.iconUrl = 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
    dataItem2.title = 'password2';
    dataItem2.subtitle = 'username2';

    const dataSection = new DataSection();
    dataSection.dataItems.push(dataItem);
    dataSection.dataItems.push(dataItem2);
    dataSection.sectionTitle = 'Passwords';
    dataSection.sectionSubtitle = '2 passwords';

    return dataSection;
  }

  if (dataType === DataType.ADDRESSES) {
    const dataItem = new Data();
    dataItem.id = 3;
    dataItem.iconUrl = '';
    dataItem.title = 'address';
    dataItem.subtitle = 'street';

    const dataSection = new DataSection();
    dataSection.dataItems.push(dataItem);
    dataSection.sectionTitle = 'Addresses';

    return dataSection;
  }

  return new DataSection();
}

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

  protected dataSections_: DataSection[] = [];
  protected dataSectionsExpanded_: boolean[] = [];

  override connectedCallback() {
    super.connectedCallback();

    this.batchUploadBrowserProxy_.callbackRouter.sendData.addListener(() => {
      // Populate data.
      // TODO(b/363204445): Currently dummy data, should be received by the
      // `sendData` callback.
      this.dataSections_.push(createDummyData(DataType.PASSWORDS));
      this.dataSectionsExpanded_.push(false);
      this.dataSections_.push(createDummyData(DataType.ADDRESSES));
      this.dataSectionsExpanded_.push(false);

      this.validateInput_();
      this.prepareOutput_();

      this.requestUpdate();

      // Requesting an update without explicitly waiting for it to finish.
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

  // Validates the input that the Ui assumes for display.
  private validateInput_() {
    // There should be at least one section.
    assert(
        this.dataSections_ !== undefined && this.dataSections_.length !== 0,
        'There should at least be one section to show.');

    // And any section should not be empty.
    for (const section of this.dataSections_) {
      assert(
          section.dataItems !== undefined && section.dataItems.length !== 0,
          'Sections should have at least one item to show.');

      const sectionItemsIdSet = new Set<number>();
      for (const item of section.dataItems) {
        assert(
            !sectionItemsIdSet.has(item.id),
            item.id + ' already exists in this section.' +
                ' An Id should be unique per section');
        sectionItemsIdSet.add(item.id);
      }
    }
  }

  private prepareOutput_() {
    // TODO(b/359797313): implement logic related to checkboxes/toggles.
  }

  protected close_() {
    this.batchUploadBrowserProxy_.handler.close();
  }

  protected getDialogSubtitle_(): string {
    // Dialog may start loading before receiving the data.
    if (!this.dataSections_ || this.dataSections_.length === 0) {
      return '';
    }

    return this.dataSections_[0]!.sectionSubtitle;
  }

  protected getSectionTitle_(section: DataSection): string {
    const numberOfSelectedItems = section.dataItems.length;
    return section.sectionTitle + ' (' + numberOfSelectedItems + ')';
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
    // TODO(b/359797313): implement checked values and link to output.
    console.error('checked: ' + e.detail);
    const currentTarget = e.currentTarget as HTMLElement;

    // Getting the index set in `data-index` property.
    const index = Number(currentTarget.dataset['index']);
    console.error('index: ' + index);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'batch-upload-app': BatchUploadAppElement;
  }
}

customElements.define(BatchUploadAppElement.is, BatchUploadAppElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './icons.html.js';
import './request.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_page_host_style.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToolbarSearchFieldElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.ts';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {PageHandlerInterface} from './suggest_internals.mojom-webui.js';
import {PageCallbackRouter, PageHandler, Request} from './suggest_internals.mojom-webui.js';

interface SuggestInternalsAppElement {
  $: {
    drawer: CrDrawerElement,
    fileInput: HTMLInputElement,
    hardcodeResponseDialog: CrDialogElement,
    toast: CrToastElement,
  };
}

// Displays the suggest requests from the most recent to the least recent.
class SuggestInternalsAppElement extends PolymerElement {
  static get is() {
    return 'suggest-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      filter_: String,
      hardcodedRequest_: Request,
      requests_: Object,
      responseDelay_: String,
      responseText_: String,
      toastDuration_: Number,
      toastMessage_: String,
    };
  }

  private filter_: string = '';
  private hardcodedRequest_: Request|null = null;
  private requests_: Request[] = [];
  private responseDelay_: string = '';
  private responseText_: string = '';
  private toastDuration_: number = 3000;
  private toastMessage_: string = '';

  private callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerInterface;
  private suggestionsRequestCompletedListenerId_: number|null = null;
  private suggestionsRequestCreatedListenerId_: number|null = null;
  private suggestionsRequestStartedListenerId_: number|null = null;


  constructor() {
    super();
    this.pageHandler_ = PageHandler.getRemote();
    this.callbackRouter_ = new PageCallbackRouter();
    this.pageHandler_.setPage(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.suggestionsRequestCreatedListenerId_ =
        this.callbackRouter_.onSuggestRequestCreated.addListener(
            this.onSuggestRequestCreated_.bind(this));
    this.suggestionsRequestStartedListenerId_ =
        this.callbackRouter_.onSuggestRequestStarted.addListener(
            this.onSuggestRequestStarted_.bind(this));
    this.suggestionsRequestCompletedListenerId_ =
        this.callbackRouter_.onSuggestRequestCompleted.addListener(
            this.onSuggestRequestCompleted_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.suggestionsRequestCreatedListenerId_);
    this.callbackRouter_.removeListener(
        this.suggestionsRequestCreatedListenerId_);
    assert(this.suggestionsRequestStartedListenerId_);
    this.callbackRouter_.removeListener(
        this.suggestionsRequestStartedListenerId_);
    assert(this.suggestionsRequestCompletedListenerId_);
    this.callbackRouter_.removeListener(
        this.suggestionsRequestCompletedListenerId_);
  }

  private millisecondsToMojoTimeDelta(milliseconds: number): TimeDelta {
    return {microseconds: BigInt(Math.floor(milliseconds * 1000))};
  }

  private onClearClick_() {
    this.requests_ = [];
    this.hardcodedRequest_ = null;
  }

  private onCloseDialogs_() {
    this.$.hardcodeResponseDialog.close();
  }

  private async onConfirmHardcodeResponseDialog_() {
    const responseDelayMs = Math.max(0, parseInt(this.responseDelay_) || 0);
    await this.pageHandler_
        .hardcodeResponse(
            this.responseText_,
            this.millisecondsToMojoTimeDelta(responseDelayMs))
        .then(({request}) => {
          this.hardcodedRequest_ = request;
        });
    this.$.hardcodeResponseDialog.close();
  }

  private onExportClick_() {
    const a = document.createElement('a');
    const file =
        new Blob([this.stringifyRequests_()], {type: 'application/json'});
    a.href = URL.createObjectURL(file);
    const iso = (new Date()).toISOString();
    iso.replace(/:/g, '').split('.')[0]!;
    a.download = `suggest_internals_export_${iso}.json`;
    a.click();
  }

  private onFilterChanged_(e: CustomEvent<string>) {
    this.filter_ = e.detail ?? '';
  }

  private onImportClick_() {
    this.$.fileInput.click();
  }

  private onImportFile_(event: Event) {
    const file = (event.target as HTMLInputElement).files?.[0];
    if (!file) {
      return;
    }

    this.readFile(file).then((importString: string) => {
      try {
        this.requests_ = JSON.parse(importString);
      } catch (error) {
        console.error('error during import, invalid json:', error);
      }
    });
  }

  private onOpenHardcodeResponseDialog_(e: CustomEvent<string>) {
    this.responseDelay_ = '';
    this.responseText_ = e.detail;
    this.$.hardcodeResponseDialog.showModal();
  }

  private onShowToast_(e: CustomEvent<string>) {
    this.toastMessage_ = e.detail;
    this.$.toast.show();
  }

  private onSuggestRequestCreated_(request: Request) {
    // Add the request to the start of the list of known requests.
    this.unshift('requests_', request);
  }

  private onSuggestRequestStarted_(request: Request) {
    const index = this.requests_.findIndex((element: Request) => {
      return request.id.high === element.id.high &&
          request.id.low === element.id.low;
    });
    // If the request is known, update it with the additional information.
    if (index !== -1) {
      this.set(`requests_.${index}.status`, request.status);
      this.set(
          `requests_.${index}.data`,
          Object.assign({}, this.requests_[index].data, request.data));
      this.set(`requests_.${index}.startTime`, request.startTime);
    }
  }

  private onSuggestRequestCompleted_(request: Request) {
    const index = this.requests_.findIndex((element: Request) => {
      return request.id.high === element.id.high &&
          request.id.low === element.id.low;
    });
    // If the request is known, update it with the additional information.
    if (index !== -1) {
      this.set(`requests_.${index}.status`, request.status);
      this.set(
          `requests_.${index}.data`,
          Object.assign({}, this.requests_[index].data, request.data));
      this.set(`requests_.${index}.endTime`, request.endTime);
      this.set(`requests_.${index}.response`, request.response);
    }
  }

  private readFile(file: File): Promise<string> {
    return new Promise(resolve => {
      const reader = new FileReader();
      reader.onloadend = () => {
        if (reader.readyState === FileReader.DONE) {
          resolve(reader.result as string);
        } else {
          console.error('error importing, unable to read file:', reader.error);
        }
      };
      reader.readAsText(file);
    });
  }

  private requestFilter_(): (request: Request) => boolean {
    const filter = this.filter_.trim().toLowerCase();
    return request => request.url.url.toLowerCase().includes(filter);
  }

  private showOutputControls_() {
    this.$.drawer.openDrawer();
  }

  private stringifyRequests_() {
    return JSON.stringify(
        this.requests_,
        (_key, value) => typeof value === 'bigint' ? value.toString() : value);
  }

  private populateSearchInput_(e: CustomEvent<string>) {
    // Populate the searchbar with the pgcl of the selected chip.
    const toolbar = this.shadowRoot!.querySelector<HTMLElement>('cr-toolbar')!;
    const searchbar =
        toolbar.shadowRoot!.querySelector<CrToolbarSearchFieldElement>(
            'cr-toolbar-search-field')!;
    searchbar.setValue('pgcl=' + e.detail);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'suggest-internals-app': SuggestInternalsAppElement;
  }
}

customElements.define(
    SuggestInternalsAppElement.is, SuggestInternalsAppElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './request.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_page_host_style.css.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PageCallbackRouter, PageHandler, PageHandlerInterface, Request} from './suggest_internals.mojom-webui.js';

interface SuggestInternalsAppElement {
  $: {
    hardcodeResponseDialog: CrDialogElement,
    toast: CrToastElement,
    viewRequestDialog: CrDialogElement,
    viewResponseDialog: CrDialogElement,
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
      responseText_: String,
      toastDuration_: Number,
      toastMessage_: String,
    };
  }

  private filter_: string = '';
  private hardcodedRequest_: Request|null;
  private requests_: Request[] = [];
  private responseText_: string = '';
  private toastDuration_: number = 3000;
  private toastMessage_: string = '';

  private callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerInterface;
  private suggestionsRequestCompletedListenerId_: number|null = null;
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
    this.suggestionsRequestStartedListenerId_ =
        this.callbackRouter_.onSuggestRequestStarting.addListener(
            this.onSuggestRequestStarting_.bind(this));
    this.suggestionsRequestCompletedListenerId_ =
        this.callbackRouter_.onSuggestRequestCompleted.addListener(
            this.onSuggestRequestCompleted_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.suggestionsRequestStartedListenerId_);
    this.callbackRouter_.removeListener(
        this.suggestionsRequestStartedListenerId_);
    assert(this.suggestionsRequestCompletedListenerId_);
    this.callbackRouter_.removeListener(
        this.suggestionsRequestCompletedListenerId_);
  }

  private onClientDataLinkClick_() {
    window.open('http://protoshop/webserver.gws.ClientDataHeader');
  }

  private onCloseDialogs_() {
    this.$.hardcodeResponseDialog.close();
    this.$.viewRequestDialog.close();
    this.$.viewResponseDialog.close();
  }

  private async onConfirmHardcodeResponseDialog_() {
    await this.pageHandler_.hardcodeResponse(this.responseText_)
        .then(({request}) => {
          this.hardcodedRequest_ = request;
        });
    this.$.hardcodeResponseDialog.close();
  }

  private onEntityInfoLinkClick_() {
    window.open('http://protoshop/gws.searchbox.chrome.EntityInfo');
  }

  private onFilterChanged_(e: CustomEvent<string>) {
    this.filter_ = e.detail ?? '';
  }

  private onGroupsInfoLinkClick_() {
    window.open('http://protoshop/gws.searchbox.chrome.GroupsInfo');
  }

  private onOpenHardcodeResponseDialog_(e: CustomEvent<string>) {
    this.responseText_ = e.detail;
    this.$.hardcodeResponseDialog.showModal();
  }

  private onOpenViewRequestDialog_() {
    this.$.viewRequestDialog.showModal();
  }

  private onOpenViewResponseDialog_() {
    this.$.viewResponseDialog.showModal();
  }

  private onShowToast_(e: CustomEvent<string>) {
    this.toastMessage_ = e.detail;
    this.$.toast.show();
  }

  private onSuggestRequestStarting_(request: Request) {
    // Add the request to the start of the list of known requests.
    this.unshift('requests_', request);
  }

  private onSuggestRequestCompleted_(request: Request) {
    const index = this.requests_.findIndex((element: Request) => {
      return request.id.high === element.id.high &&
          request.id.low === element.id.low;
    });
    // If the request is known, update it with the additional information.
    if (index !== -1) {
      this.set(`requests_.${index}.status`, request.status);
      this.set(`requests_.${index}.endTime`, request.endTime);
      this.set(`requests_.${index}.response`, request.response);
    }
  }

  private requestFilter_(): (request: Request) => boolean {
    const filter = this.filter_.trim().toLowerCase();
    return request => request.url.url.toLowerCase().includes(filter);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'suggest-internals-app': SuggestInternalsAppElement;
  }
}

customElements.define(
    SuggestInternalsAppElement.is, SuggestInternalsAppElement);

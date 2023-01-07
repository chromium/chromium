// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DocumentMetadata} from '../constants.js';

import {getTemplate} from './viewer-properties-dialog.html.js';

export interface ViewerPropertiesDialogElement {
  $: {
    dialog: CrDialogElement,
    close: HTMLElement,
  };
}

export class ViewerPropertiesDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-properties-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      documentMetadata: Object,
      fileName: String,
      pageCount: Number,
    };
  }

  documentMetadata: DocumentMetadata;
  fileName: string;
  pageCount: number;

  private getFastWebViewValue_(
      yesLabel: string, noLabel: string, linearized: boolean): string {
    return linearized ? yesLabel : noLabel;
  }

  private getOrPlaceholder_(value: string): string {
    return value || '-';
  }

  private onClickClose_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-properties-dialog': ViewerPropertiesDialogElement;
  }
}

customElements.define(
    ViewerPropertiesDialogElement.is, ViewerPropertiesDialogElement);

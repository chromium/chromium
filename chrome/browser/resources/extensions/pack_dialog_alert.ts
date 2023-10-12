// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pack_dialog_alert.html.js';

export interface ExtensionsPackDialogAlertElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class ExtensionsPackDialogAlertElement extends PolymerElement {
  static get is() {
    return 'extensions-pack-dialog-alert';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
      title_: String,
      message_: String,
      cancelLabel_: String,
      confirmLabel_: String,
    };
  }

  private title_: string;
  private message_: string;
  private cancelLabel_: string|null = null;
  /** This needs to be initialized to trigger data-binding. */
  private confirmLabel_: string|null = '';
  model: chrome.developerPrivate.PackDirectoryResponse;

  get returnValue(): string {
    return this.$.dialog.getNative().returnValue;
  }

  override ready() {
    super.ready();

    // Initialize button label values for initial html binding.
    this.cancelLabel_ = null;
    this.confirmLabel_ = null;

    switch (this.model.status) {
      case chrome.developerPrivate.PackStatus.WARNING:
        this.title_ = loadTimeData.getString('packDialogWarningTitle');
        this.cancelLabel_ = loadTimeData.getString('cancel');
        this.confirmLabel_ = loadTimeData.getString('packDialogProceedAnyway');
        break;
      case chrome.developerPrivate.PackStatus.ERROR:
        this.title_ = loadTimeData.getString('packDialogErrorTitle');
        this.cancelLabel_ = loadTimeData.getString('ok');
        break;
      case chrome.developerPrivate.PackStatus.SUCCESS:
        this.title_ = loadTimeData.getString('packDialogTitle');
        this.cancelLabel_ = loadTimeData.getString('ok');
        break;
      default:
        assertNotReached();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private getCancelButtonClass_(): string {
    return this.confirmLabel_ ? 'cancel-button' : 'action-button';
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onConfirmClick_() {
    // The confirm button should only be available in WARNING state.
    assert(this.model.status === chrome.developerPrivate.PackStatus.WARNING);
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-pack-dialog-alert': ExtensionsPackDialogAlertElement;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-pack-dialog-alert': ExtensionsPackDialogAlertElement;
  }
}

customElements.define(
    ExtensionsPackDialogAlertElement.is, ExtensionsPackDialogAlertElement);

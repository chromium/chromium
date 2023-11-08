// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'bypass-warning-confirmation-dialog' is the dialog that allows
 * bypassing a download warning (keeping a file flagged as dangerous). A
 * 'success' indicates the warning bypass was confirmed and the dangerous file
 * was downloaded.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bypass_warning_confirmation_dialog.html.js';


export interface DownloadBypassWarningConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const DownloadBypassWarningConfirmationDialogBase = PolymerElement;

export class DownloadBypassWarningConfirmationDialogElement extends
    DownloadBypassWarningConfirmationDialogBase {
  static get is() {
    return 'download-bypass-warning-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      fileName: String,
    };
  }

  fileName: string;

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onDownloadDangerousClick_() {
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'download-bypass-warning-confirmation-dialog':
        DownloadBypassWarningConfirmationDialogElement;
  }
}

customElements.define(
    DownloadBypassWarningConfirmationDialogElement.is,
    DownloadBypassWarningConfirmationDialogElement);

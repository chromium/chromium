// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'downloads-bypass-warning-confirmation-dialog' is the dialog
 * that allows bypassing a download warning (keeping a file flagged as
 * dangerous). A 'success' indicates the warning bypass was confirmed and the
 * dangerous file was downloaded.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './bypass_warning_confirmation_dialog.css.js';
import {getHtml} from './bypass_warning_confirmation_dialog.html.js';


export interface DownloadsBypassWarningConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class DownloadsBypassWarningConfirmationDialogElement extends
    CrLitElement {
  static get is() {
    return 'downloads-bypass-warning-confirmation-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fileName: {type: String},
    };
  }

  fileName: string = '';

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected onDownloadDangerousClick_() {
    getAnnouncerInstance().announce(
        loadTimeData.getString('screenreaderSavedDangerous'));
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'downloads-bypass-warning-confirmation-dialog':
        DownloadsBypassWarningConfirmationDialogElement;
  }
}

customElements.define(
    DownloadsBypassWarningConfirmationDialogElement.is,
    DownloadsBypassWarningConfirmationDialogElement);

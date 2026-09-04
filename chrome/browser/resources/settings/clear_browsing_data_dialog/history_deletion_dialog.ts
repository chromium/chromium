// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-history-deletion-dialog' is a dialog that is
 * optionally shown inside settings-clear-browsing-data-dialog after deleting
 * browsing history. It informs the user about the existence of other forms
 * of browsing history in their account.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './history_deletion_dialog.html.js';

export interface SettingsHistoryDeletionDialogElement {
  $: {
    dialog: CrDialogElement,
    okButton: CrButtonElement,
  };
}

export type HistoryDeletionDialogElement = SettingsHistoryDeletionDialogElement;

export class SettingsHistoryDeletionDialogElement extends CrLitElement {
  static get is() {
    return 'settings-history-deletion-dialog';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  /** Click handler for the "OK" button. */
  protected onOkClick_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-history-deletion-dialog': SettingsHistoryDeletionDialogElement;
  }
}

customElements.define(
    SettingsHistoryDeletionDialogElement.is,
    SettingsHistoryDeletionDialogElement);

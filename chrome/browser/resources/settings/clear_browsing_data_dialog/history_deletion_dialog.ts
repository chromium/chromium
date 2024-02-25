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
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_deletion_dialog.html.js';

export interface SettingsHistoryDeletionDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsHistoryDeletionDialogElement extends PolymerElement {
  static get is() {
    return 'settings-history-deletion-dialog';
  }

  static get template() {
    return getTemplate();
  }

  /** Click handler for the "OK" button. */
  private onOkClick_() {
    this.$.dialog.close();
  }
}

customElements.define(
    SettingsHistoryDeletionDialogElement.is,
    SettingsHistoryDeletionDialogElement);

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-passwords-deletion-dialog' is a dialog that is
 * optionally shown inside settings-clear-browsing-data-dialog after deleting
 * passwords. It informs the user that not all password deletions were completed
 * successfully.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passwords_deletion_dialog.html.js';

export interface SettingsPasswordsDeletionDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsPasswordsDeletionDialogElement extends PolymerElement {
  static get is() {
    return 'settings-passwords-deletion-dialog';
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
    SettingsPasswordsDeletionDialogElement.is,
    SettingsPasswordsDeletionDialogElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';
import '../user_utils_mixin.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {UserUtilMixin} from '../user_utils_mixin.js';

import {getTemplate} from './move_passwords_dialog.html.js';

export interface MovePasswordsDialogElement {
  $: {
    accountEmail: HTMLElement,
    avatar: HTMLImageElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    move: CrButtonElement,
  };
}

const MovePasswordsDialogElementBase = UserUtilMixin(PolymerElement);

export class MovePasswordsDialogElement extends MovePasswordsDialogElementBase {
  static get is() {
    return 'move-passwords-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Password groups displayed in the UI.
       */
      passwords: {
        type: Array,
        value: () => [],
      },
    };
  }

  passwords: chrome.passwordsPrivate.PasswordUiEntry[];

  private onCancel_() {
    this.$.dialog.close();
  }

  private onMoveButtonClick_() {
    assert(this.isOptedInForAccountStorage);
    // TODO(crbug.com/1420919): Implement password moving.
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-passwords-dialog': MovePasswordsDialogElement;
  }
}

customElements.define(
    MovePasswordsDialogElement.is, MovePasswordsDialogElement);

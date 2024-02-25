// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-disk-resize-confirmation-dialog' is a
 * component warning the user that resizing a sparse disk cannot be undone.
 * By clicking 'Reserve size', the user agrees to start the operation.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './crostini_disk_resize_confirmation_dialog.html.js';

interface SettingsCrostiniDiskResizeConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsCrostiniDiskResizeConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-crostini-disk-resize-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.getDialog_().showModal();
  }

  private onCancelClick_(): void {
    this.getDialog_().cancel();
  }

  private onReserveSizeClick_(): void {
    this.getDialog_().close();
  }

  private getDialog_(): CrDialogElement {
    return this.$.dialog;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-disk-resize-confirmation-dialog':
        SettingsCrostiniDiskResizeConfirmationDialogElement;
  }
}

customElements.define(
    SettingsCrostiniDiskResizeConfirmationDialogElement.is,
    SettingsCrostiniDiskResizeConfirmationDialogElement);

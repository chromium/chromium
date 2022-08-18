// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-disk-resize-confirmation-dialog' is a
 * component warning the user that resizing a sparse disk cannot be undone.
 * By clicking 'Reserve size', the user agrees to start the operation.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsCrostiniDiskResizeConfirmationDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-crostini-disk-resize-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.getDialog_().showModal();
  }

  /** @private */
  onCancelTap_() {
    this.getDialog_().cancel();
  }

  /** @private */
  onReserveSizeTap_() {
    this.getDialog_().close();
  }

  /**
   * @private
   * @return {!CrDialogElement}
   */
  getDialog_() {
    return /** @type{!CrDialogElement} */ (this.$.dialog);
  }
}

customElements.define(
    SettingsCrostiniDiskResizeConfirmationDialogElement.is,
    SettingsCrostiniDiskResizeConfirmationDialogElement);

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import './firmware_shared_css.js';
import './firmware_shared_fonts.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FirmwareUpdate, UpdatePriority} from './firmware_update_types.js';

/** @enum {number} */
export const DialogState = {
  CLOSED: 0,
  DEVICE_PREP: 1,
};

/**
 * @fileoverview
 * 'update-card' displays information about a peripheral update.
 */
export class UpdateCardElement extends PolymerElement {
  static get is() {
    return 'update-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!FirmwareUpdate} */
      update: {
        type: Object,
      },

      /** @protected {!DialogState} */
      dialogState_: {
        type: Number,
        value: DialogState.CLOSED,
      },
    };
  }

  /**
   * @protected
   * @return {boolean}
   */
  isCriticalUpdate_() {
    return this.update.priority === UpdatePriority.kCritical;
  }

  /** @protected */
  onUpdateButtonClicked_() {
    if (this.update.updateModeInstructions) {
      this.dialogState_ = DialogState.DEVICE_PREP;
    }
    // TODO(michaelcheco): Show update dialog immediately if no instructions
    // are provided.
  }

  /** @protected */
  closeDialog_() {
    this.dialogState_ = DialogState.CLOSED;
  }

  /** @protected */
  startUpdate_() {
    // TODO(michaelcheco): Add implementation.
  }

  /**
   * @protected
   * @return {boolean}
   */
  shouldShowDevicePrepDialog_() {
    return this.dialogState_ === DialogState.DEVICE_PREP;
  }
}

customElements.define(UpdateCardElement.is, UpdateCardElement);

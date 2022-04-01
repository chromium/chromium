// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import './shortcut_customization_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ViewState} from './accelerator_view.js';
import {AcceleratorInfo, AcceleratorSource} from './shortcut_types.js';

/**
 * @fileoverview
 * 'accelerator-edit-dialog' is a dialog that displays the accelerators for
 * a given shortcut. Allows users to edit the accelerators.
 * TODO(jimmyxgong): Implement editing accelerators.
 */
export class AcceleratorEditDialogElement extends PolymerElement {
  static get is() {
    return 'accelerator-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      description: {
        type: String,
        value: '',
      },

      /** @type {!Array<!AcceleratorInfo>} */
      acceleratorInfos: {
        type: Array,
        value: () => {},
      },

      /** @private */
      pendingNewAcceleratorState_: {
        type: Number,
        value: ViewState.VIEW,
      },

      action: {
        type: Number,
        value: 0,
      },

      /** @type {!AcceleratorSource} */
      source: {
        type: Number,
        value: 0,
      },

      /** @protected */
      isAcceleratorCapturing_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /**
     * Event callback for 'accelerator-capturing-started'.
     * @private {!Function}
     */
    this.onAcceleratorCapturingStarted_ = () => {
      this.isAcceleratorCapturing_ = true;
    };

    /**
     * Event callback for 'accelerator-capturing-ended'.
     * @private {!Function}
     */
    this.onAcceleratorCapturingEnded_ = () => {
      this.isAcceleratorCapturing_ = false;
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.editDialog.showModal();

    window.addEventListener(
        'accelerator-capturing-started', this.onAcceleratorCapturingStarted_);
    window.addEventListener(
        'accelerator-capturing-ended', this.onAcceleratorCapturingEnded_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener(
        'accelerator-capturing-started', this.onAcceleratorCapturingStarted_);
    window.removeEventListener(
        'accelerator-capturing-ended', this.onAcceleratorCapturingEnded_);
  }

  /**
   * @param {!Array<AcceleratorInfo>} updatedAccels
   */
  updateDialogAccelerators(updatedAccels) {
    this.set('acceleratorInfos', []);
    this.shadowRoot.querySelector('#viewList').render();
    this.acceleratorInfos = updatedAccels;
  }

  /** @protected */
  onDoneButtonClicked_() {
    this.$.editDialog.close();
  }

  /** @protected */
  onDialogClose_() {
    this.dispatchEvent(new CustomEvent('edit-dialog-closed',
        {bubbles: true, composed: true}));
  }

  /** @protected */
  onAddAcceleratorClicked_() {
    this.pendingNewAcceleratorState_ = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    const editView = this.$.editDialog.querySelector('#pendingAccelerator');
    const accelItem = editView.shadowRoot.querySelector('#acceleratorItem');
    accelItem.shadowRoot.querySelector('#container').focus();
  }

  /**
   * @return {boolean}
   * @protected
   */
  showAddButton_() {
    // If the state is VIEW, no new pending accelerators are being added.
    return this.pendingNewAcceleratorState_ === ViewState.VIEW;
  }

  /** @protected */
  onRestoreDefaultButtonClicked_() {
    // TODO(jimmyxgong): Implement this function.
  }
}

customElements.define(AcceleratorEditDialogElement.is,
                      AcceleratorEditDialogElement);

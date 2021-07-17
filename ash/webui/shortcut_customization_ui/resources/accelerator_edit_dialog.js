// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js'

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

      /**
       * TODO(jimmyxgong): Replace with proper mojom::Accelerator type and
       * implement fetching the accelerators for this row.
       * @type {!Array<!Object>}
       */
      accelerators: {
        type: Array,
        value: () => {},
      }
    }
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.editDialog.showModal();
  }

  onDoneButtonClicked() {
    this.$.editDialog.close();
  }
}

customElements.define(AcceleratorEditDialogElement.is,
                      AcceleratorEditDialogElement);
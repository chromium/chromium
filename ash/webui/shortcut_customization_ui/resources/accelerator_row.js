// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js'

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'accelerator-row' is a wrapper component for one shortcut. It features a
 * description of the shortcut along with a list of accelerators.
 * TODO(jimmyxgong): Implement opening a dialog when clicked.
 */
export class AcceleratorRowElement extends PolymerElement {
  static get is() {
    return 'accelerator-row';
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
}

customElements.define(AcceleratorRowElement.is, AcceleratorRowElement);
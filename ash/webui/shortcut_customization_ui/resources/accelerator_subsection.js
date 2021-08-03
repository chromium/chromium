// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_row.js'

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'accelerator-subsection' is a wrapper component for a subsection of
 * shortcuts.
 */
export class AcceleratorSubsectionElement extends PolymerElement {
  static get is() {
    return 'accelerator-subsection';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      title: {
        type: String,
        value: '',
      },

      /**
       * TODO(jimmyxgong): Fetch the shortcuts and it accelerators with the
       * mojom::source_id and mojom::subsection_id. This serves as a temporary
       * way to populate a subsection.
       * @type {!Array<!{string, !Array<!AcceleratorInfo>}>}
       */
      acceleratorContainer: {
        type: Array,
        value: () => {},
      }
    }
  }
}

customElements.define(AcceleratorSubsectionElement.is,
                      AcceleratorSubsectionElement);
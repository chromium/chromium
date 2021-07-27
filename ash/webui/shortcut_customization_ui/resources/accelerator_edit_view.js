// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js'
import './icons.js'

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'accelerator-edit-view' is a wrapper component for one accelerator. It is
 * responsible for displaying the edit/remove buttons to an accelerator and also
 * displaying context or errors strings for an accelerator.
 */
export class AcceleratorEditViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-edit-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * TODO(jimmyxgong): Replace with proper mojom::Accelerator type.
       * @type {!Object}
       */
      accelerator: {
        type: Object,
        value: () => {},
      },

      isEditView: {
        type: Boolean,
        value: false,
        notify:true,
        reflectToAttribute: true,
      },
    }
  }

  /** @private */
  onEditButtonClicked_() {
    this.isEditView = true;
  }

  /** @private */
  onDeleteButtonClicked_() {
    // TODO(jimmyxgong): Implement this function
  }

  /** @private  */
  onCancelButtonClicked_() {
    this.isEditView = false;
  }
}

customElements.define(AcceleratorEditViewElement.is,
                      AcceleratorEditViewElement);
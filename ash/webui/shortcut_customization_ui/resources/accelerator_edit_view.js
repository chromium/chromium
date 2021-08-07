// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js'
import './icons.js'
import './shortcut_customization_shared_css.js';

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorKeys, AcceleratorInfo, AcceleratorState, AcceleratorType} from './shortcut_types.js';

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
      /** @type {!AcceleratorInfo} */
      acceleratorInfo: {
        type: Object,
        value: /** @type {!AcceleratorInfo} */ ({
            accelerator: /** @type {!AcceleratorKeys} */ ({
              modifiers: 0,
              key: 0,
              key_display: '',
            }),
            type: AcceleratorType.kDefault,
            state: AcceleratorState.kEnabled,
            locked: false,
        }),
      },

      isEditView: {
        type: Boolean,
        value: false,
        notify:true,
        reflectToAttribute: true,
      },
    }
  }

  /** @protected */
  onEditButtonClicked_() {
    this.isEditView = true;
  }

  /** @protected */
  onDeleteButtonClicked_() {
    // TODO(jimmyxgong): Implement this function
  }

  /** @protected  */
  onCancelButtonClicked_() {
    this.isEditView = false;
  }
}

customElements.define(AcceleratorEditViewElement.is,
                      AcceleratorEditViewElement);
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js';
import './icons.js';
import './shortcut_customization_shared_css.js';

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorInfo, AcceleratorSource, ShortcutProviderInterface} from './shortcut_types.js';


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

      /** @type {!Array<!AcceleratorInfo>} */
      acceleratorInfos: {
        type: Array,
        value: () => {},
      },

      /** @private */
      isLocked_: {
        type: Boolean,
        value: false,
      },

      /** @type {number} */
      action: {
        type: Number,
        value: 0,
      },

      /** @type {!AcceleratorSource} */
      source: {
        type: Number,
        value: 0,
        observer: 'onSourceChanged_',
      },
    };
  }

  constructor() {
    super();
    /** @private {!ShortcutProviderInterface} */
    this.shortcutInterfaceProvider_ = getShortcutProvider();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    if (!this.isLocked_) {
      this.removeEventListener('click', () => this.showDialog_());
    }
  }

  /** @protected */
  onSourceChanged_() {
    this.shortcutInterfaceProvider_.isMutable(this.source).then((result) => {
      this.isLocked_ = !result;
      if (!this.isLocked_) {
        this.addEventListener('click', () => this.showDialog_());
      }
    });
  }

  /** @private */
  showDialog_() {
    this.dispatchEvent(new CustomEvent(
        'show-edit-dialog',
        {
          bubbles: true,
          composed: true,
          detail: {
            description: this.description,
            accelerators: this.acceleratorInfos,
            action: this.action,
            source: this.source
          }
        },
        ));
  }
}

customElements.define(AcceleratorRowElement.is, AcceleratorRowElement);
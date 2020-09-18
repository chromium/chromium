// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ModuleDescriptor} from './module_descriptor.js';

/** @fileoverview Element that implements the common module UI. */

class ModuleWrapperElement extends PolymerElement {
  static get is() {
    return 'ntp-module-wrapper';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!ModuleDescriptor} */
      descriptor: {
        observer: 'onDescriptorChange_',
        type: Object,
      },
    };
  }

  /** @private */
  onDescriptorChange_() {
    this.$.moduleElement.innerHTML = '';
    this.$.moduleElement.appendChild(this.descriptor.element);
    this.$.moduleElement.style.height = `${this.descriptor.heightPx}px`;
  }
}

customElements.define(ModuleWrapperElement.is, ModuleWrapperElement);

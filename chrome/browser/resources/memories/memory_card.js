// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {Memory} from '/components/memories/core/memories.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a Memory.
 */

class MemoryCardElement extends PolymerElement {
  static get is() {
    return 'memory-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * The Memory displayed by this element.
       * @type {!Memory}
       */
      memory: Object,
    };
  }
}

customElements.define(MemoryCardElement.is, MemoryCardElement);

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a tile with
 * slots for a main section at the top and primary and secondary captions below.
 */

class MemoryTileElement extends PolymerElement {
  static get is() {
    return 'memory-tile';
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
       * Optional URL to navigate to when the tile is clicked.
       * @type {Url}
       */
      url: Object,
    };
  }
}

customElements.define(MemoryTileElement.is, MemoryTileElement);

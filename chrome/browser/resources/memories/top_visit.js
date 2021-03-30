// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.js';
import './visit_row.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {Visit} from '/components/history_clusters/core/memories.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview This file provides a custom element displaying a top visit
 * within a Memory. A top visit is a featured, i.e., visible, visit with an
 * optional set of related visits which are not visible by default.
 */

class TopVisitElement extends PolymerElement {
  static get is() {
    return 'top-visit';
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
       * The top visit to display
       * @type {!Visit}
       */
      visit: Object,

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the related visits of the top visit are expanded/visible.
       * @private {boolean}
       */
      expanded_: Boolean,
    };
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!CustomEvent<{event:!MouseEvent}>} e
   * @private
   */
  onVisitClick_(e) {
    // Prevent the enclosing <cr-expand-button> from receiving this event.
    e.detail.event.stopImmediatePropagation();
  }
}

customElements.define(TopVisitElement.is, TopVisitElement);

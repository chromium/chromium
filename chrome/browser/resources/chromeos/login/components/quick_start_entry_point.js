// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *   @fileoverview
 *   Material design button that shows Android phone icon and displays text to
 *   use quick start.
 *
 *   Example:
 *     <quick-start-entry-point
 *       quickStartTextkey="welcomeScreenQuickStart">
 *     </quick-start-entry-point>
 *
 *   Attributes:
 *     'quickStartTextkey' - ID of localized string to be used as button text.
 */


import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class QuickStartEntryPoint extends PolymerElement {
  static get is() {
    return 'quick-start-entry-point';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      quickStartTextKey: {
        type: String,
        value: '',
      },
    };
  }
}

customElements.define(QuickStartEntryPoint.is, QuickStartEntryPoint);

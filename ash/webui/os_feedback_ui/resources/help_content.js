// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './os_feedback_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'help-content' displays list of help contents.
 */
export class HelpContentElement extends PolymerElement {
  static get is() {
    return 'help-content';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(HelpContentElement.is, HelpContentElement);

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'base-page' is the main page for the shimless rma process modal dialog.
 */
export class BasePageElement extends PolymerElement {
  static get is() {
    return 'base-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  constructor() {
    super();

    /**
     * This property is used by the CSS to set the height of the right pane.
     * The height of the right pane needs to be set to a fixed number to make
     * scrollable regions display properly. This has to be done in javascript
     * because there is no way to get screen height in CSS.
     * @type {number}
     * */
    const windowHeight = window.innerHeight;
    this.style.setProperty('--window-height', windowHeight.toString() + 'px');
  }

  /** @override */
  ready() {
    super.ready();
  }
}

customElements.define(BasePageElement.is, BasePageElement);

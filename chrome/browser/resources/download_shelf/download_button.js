// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Button UI for "Discard" and "Show All".
 */

import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

export class DownloadButtonElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();
  }
}

customElements.define('download-button', DownloadButtonElement);

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './download_list.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

export class DownloadShelfAppElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }
}

customElements.define('download-shelf-app', DownloadShelfAppElement);

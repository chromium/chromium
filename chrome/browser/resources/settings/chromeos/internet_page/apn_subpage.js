// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing APNs.
 */

import './internet_shared_css.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class ApnSubpageElement extends PolymerElement {
  static get is() {
    return 'apn-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ApnSubpageElement.is, ApnSubpageElement);
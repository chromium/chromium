// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './shared_style.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class ExtensionsCheckupElement extends PolymerElement {
  static get is() {
    return 'extensions-checkup';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ExtensionsCheckupElement.is, ExtensionsCheckupElement);

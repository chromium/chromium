// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ReadLaterItemElement extends PolymerElement {
  static get is() {
    return 'read-later-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!readLater.mojom.ReadLaterEntry} */
      data: Object,
    };
  }
}

customElements.define(ReadLaterItemElement.is, ReadLaterItemElement);
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './spinner_page.html.js';

export class SpinnerPageElement extends PolymerElement {
  static get is() {
    return 'spinner-page';
  }

  static get template() {
    return getTemplate();
  }

  private onCancelClick_() {
    // Send cancel signal to Chrome C++ side using BrowserProxy. It will be
    // added in follow-up CL.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'spinner-page': SpinnerPageElement;
  }
}

customElements.define(SpinnerPageElement.is, SpinnerPageElement);
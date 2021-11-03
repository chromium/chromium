// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './base_page.js';
import './icons.js';
import './shimless_rma_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'splash-screen' is displayed while waiting for the first state to be fetched
 * by getCurrentState.
 */
export class SplashScreen extends PolymerElement {
  static get is() {
    return 'splash-screen';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(SplashScreen.is, SplashScreen);

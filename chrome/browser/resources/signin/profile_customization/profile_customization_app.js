// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


Polymer({
  is: 'profile-customization-app',

  _template: html`{__html_template__}`,

  /** @private */
  onDone_() {
    // TODO: call native to close the bubble
  },
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
/**
 * @fileoverview Polymer element for displaying error screens with error icon,
 * title and body text.
 */

Polymer({
  is: 'account-manager-error-screen',

  _template: html`{__html_template__}`,

  properties: {
    errorTitle: {
      type: String,
      value: '',
    },
  },
});

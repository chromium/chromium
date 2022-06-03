// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'edu-login-parent-info',

  _template: html`{__html_template__}`,

  listeners: {
    'view-enter-finish': 'viewIsActive_',
  },

  /**
   * Called when current view becomes active. Update the contents to set correct
   * scollable classes.
   * @private
   */
  viewIsActive_() {
    this.$$('edu-login-template').updateScrollableContents();
  }
});

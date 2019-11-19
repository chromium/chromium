// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'viewer-error-screen',

  _template: html`{__html_template__}`,

  properties: {
    reloadFn: Function,

    strings: Object,
  },

  show: function() {
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  reload: function() {
    if (this.reloadFn) {
      this.reloadFn();
    }
  }
});

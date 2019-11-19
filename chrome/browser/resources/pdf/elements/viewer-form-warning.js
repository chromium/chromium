// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'viewer-form-warning',

  _template: html`{__html_template__}`,

  properties: {
    strings: Object,
  },

  /** @private {PromiseResolver} */
  resolver_: null,

  show: function() {
    this.resolver_ = new PromiseResolver();
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
    return this.resolver_.promise;
  },

  onCancel: function() {
    this.resolver_.reject();
    this.$.dialog.cancel();
  },

  onAction: function() {
    this.resolver_.resolve();
    this.$.dialog.close();
  },
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceController} from './edu_coexistence_controller.js';


Polymer({
  is: 'edu-coexistence-ui',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Indicates whether the page is loading.
     * @private {boolean}
     */
    loading_: {
      type: Boolean,
      value: true,
    },

    /**
     * The EDU Ceoxistence controller instance.
     * @private {?EduCoexistenceController}
     */
    controller_: Object,
  },

  /** Attempts to close the dialog */
  closeDialog_() {
    // TODO(danan): call InlineLoginDialog's "Close"
  },

  loadEduCoexistenceController_(data) {
    // TODO(danan):  use data to parameterize the webview.

    const webview =
        /** @type {!WebView} */ (this.$.signinFrame);

    // Set up the controller.
    this.controller_ = new EduCoexistenceController(webview, data);
    this.controller_.load();
    webview.addEventListener('contentload', () => {
      this.loading_ = false;
    });
    webview.addEventListener('loadstart', () => {
      this.loading_ = true;
    });
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'coexistence-data', data => this.loadEduCoexistenceController_(data));
  },
});

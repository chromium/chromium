// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthParams} from '../../gaia_auth_host/authenticator.m.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';
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
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  },

  /**
   * @param {!AuthParams} data parameters for auth extension.
   * @private
   */
  loadAuthExtension_(data) {
    // Set up the controller.
    this.controller_.loadAuthExtension(data);

    this.webview_.addEventListener('contentload', () => {
      this.loading_ = false;
    });
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));

    EduCoexistenceBrowserProxyImpl.getInstance().initializeEduArgs().then(
        (data) => {
          this.webview_ =
              /** @type {!WebView} */ (this.$.signinFrame);
          this.controller_ = new EduCoexistenceController(this.webview_, data);

          EduCoexistenceBrowserProxyImpl.getInstance().initializeLogin();
        },
        (err) => {
          console.error('There was an error getting edu coexistence data');
        });
  },
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_ui.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


// import {EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
// import {EduCoexistenceFlowResult, EduLoginErrorType, EduLoginParams,
// ParentAccount} from './edu_login_util.js';
//

Polymer({
  is: 'edu-coexistence-app',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Whether the error screen should be shown.
     * @private {boolean}
     */
    isErrorShown_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  created() {},

  onEduAccountAdded_(data) {
    // TODO(danan): show the final "Account added" screen".
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'edu-account-added', data => this.onEduAccountAdded_(data));

    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView('edu-coexistence-ui');
  },

});

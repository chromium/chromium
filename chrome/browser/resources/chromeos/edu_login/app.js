// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_login_coexistence_info.js';
import './edu_login_parents.js';
import './edu_login_parent_signin.js';
import './edu_login_parent_info.js';
import './edu_login_signin.js';
import './edu_login_error.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
import {EduLoginErrorType, EduLoginParams, ParentAccount} from './edu_login_util.js';

/** @enum {string} */
const Steps = {
  PARENTS: 'parents',
  PARENT_SIGNIN: 'parent-signin',
  COEXISTENCE_INFO: 'coexistence-info',
  PARENT_INFO: 'parent-info',
  EDU_LOGIN: 'edu-login'
};

/** @type {!Array<Steps>} */
const stepsArray = Object.values(Steps);

Polymer({
  is: 'edu-login-app',

  _template: html`{__html_template__}`,

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Steps: {
      type: Object,
      value: Steps,
    },

    /**
     * Index of the current step displayed.
     * @private {number}
     */
    stepIndex_: {
      type: Number,
      value: 0,
    },

    /**
     * Selected parent account for approving EDU login flow.
     * @private {?ParentAccount}
     */
    selectedParent_: Object,

    /**
     * Login params containing obfuscated Gaia id and Reauth Proof Token of the
     * parent who is approving EDU login flow.
     * @private {?EduLoginParams}
     */
    loginParams_: Object,

    /**
     * Whether the error screen should be shown.
     * @private {boolean}
     */
    isErrorShown_: {
      type: Boolean,
      value: false,
    },

    /** @private {EduLoginErrorType} */
    errorType_: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'go-next': 'onGoNext_',
    'go-back': 'onGoBack_',
    'edu-login-error': 'onError_',
  },

  /** @override */
  ready() {
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /**
   * Switches to the next view.
   * @private
   */
  onGoNext_() {
    assert(this.stepIndex_ < stepsArray.length - 1);
    ++this.stepIndex_;
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /**
   * Switches to the previous view.
   * @private
   */
  onGoBack_() {
    assert(this.stepIndex_ > 0);
    --this.stepIndex_;
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /**
   * Switches to the specified step.
   * @param {number} index of the step to be shown.
   * @private
   */
  switchViewAtIndex_(index) {
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(stepsArray[index]);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onError_(e) {
    this.errorType_ = e.detail.errorType;
    this.isErrorShown_ = true;
  },
});

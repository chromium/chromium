// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './shared_style.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';

Polymer({
  is: 'phonehub-tab',

  _template: html`{__html_template__}`,

  properties: {
    shouldEnableFakePhoneHubManager_: {
      type: Boolean,
      value: false,
      observer: 'onShouldEnableFakePhoneHubManagerChanged_'
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @private */
  onShouldEnableFakePhoneHubManagerChanged_() {
    this.browserProxy_.setFakePhoneHubManagerEnabled(
        this.shouldEnableFakePhoneHubManager_);
  },
});

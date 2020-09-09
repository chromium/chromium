// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './signin_icons.js';
import './signin_shared_css.js';
import './signin_vars_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AccountInfo, DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl, InterceptionParameters} from './dice_web_signin_intercept_browser_proxy.js';

Polymer({
  is: 'dice-web-signin-intercept-app',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {InterceptionParameters} */
    InterceptionParameters_: Object,
  },

  /** @private {?DiceWebSigninInterceptBrowserProxy} */
  diceWebSigninInterceptBrowserProxy_: null,

  /** @override */
  attached() {
    this.diceWebSigninInterceptBrowserProxy_ =
        DiceWebSigninInterceptBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'interception-parameters-changed',
        this.handleParametersChanged_.bind(this));
    this.diceWebSigninInterceptBrowserProxy_.pageLoaded().then(
        parameters => this.handleParametersChanged_(parameters));
  },

  /** @private */
  onAccept_() {
    this.diceWebSigninInterceptBrowserProxy_.accept();
  },

  /** @private */
  onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  },

  /**
   * Called when the interception parameters are updated.
   * @param {!InterceptionParameters} parameters
   * @private
   */
  handleParametersChanged_(parameters) {
    this.interceptionParameters_ = parameters;
    this.style.setProperty(
        '--header-background-color', parameters.headerBackgroundColor);
    this.style.setProperty('--header-text-color', parameters.headerTextColor);
    this.notifyPath('interceptionParameters_.interceptedAccount.isManaged');
  },
});

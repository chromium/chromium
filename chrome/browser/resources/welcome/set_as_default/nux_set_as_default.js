// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// <if expr="is_win">
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
// </if>
import '../shared/animations_css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateTo, navigateToNextStep, NavigationBehavior, Routes} from '../navigation_behavior.js';
import {DefaultBrowserInfo, stepIndicatorModel} from '../shared/nux_types.js';

import {NuxSetAsDefaultProxy, NuxSetAsDefaultProxyImpl} from './nux_set_as_default_proxy.js';

Polymer({
  is: 'nux-set-as-default',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    NavigationBehavior,
  ],

  properties: {
    /** @type {stepIndicatorModel} */
    indicatorModel: Object,

    // <if expr="is_win">
    isWin10: {
      type: Boolean,
      value: loadTimeData.getBoolean('is_win10'),
    },
    // </if>
  },

  /** @private {NuxSetAsDefaultProxy} */
  browserProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @private {!Function} */
  navigateToNextStep_: navigateToNextStep,

  /** @override */
  ready: function() {
    this.browserProxy_ = NuxSetAsDefaultProxyImpl.getInstance();

    this.addWebUIListener(
        'browser-default-state-changed',
        this.onDefaultBrowserChange_.bind(this));
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.browserProxy_.recordPageShown();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAwayThroughBrowserHistory();
  },

  onRouteUnload: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAway();
  },

  /** @private */
  onDeclineClick_: function() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordSkip();
    this.finished_();
  },

  /** @private */
  onSetDefaultClick_: function() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordBeginSetDefault();
    this.browserProxy_.setAsDefault();
  },

  /**
   * Automatically navigate to the next onboarding step once default changed.
   * @param {!DefaultBrowserInfo} status
   * @private
   */
  onDefaultBrowserChange_: function(status) {
    if (status.isDefault) {
      this.browserProxy_.recordSuccessfullySetDefault();
      // Triggers toast in the containing welcome-app.
      this.fire('default-browser-change');
      this.finished_();
      return;
    }

    // <if expr="is_macosx">
    // On Mac OS, we do not get a notification when the default browser changes.
    // This will fake the notification.
    window.setTimeout(() => {
      this.browserProxy_.requestDefaultBrowserState().then(
          this.onDefaultBrowserChange_.bind(this));
    }, 100);
    // </if>
  },

  /** @private */
  finished_: function() {
    this.finalized_ = true;
    this.navigateToNextStep_();
  },
});

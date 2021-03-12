// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-default-browser-page' is the settings page that contains
 * settings to change the default browser (i.e. which the OS will open).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../icons.js';
import '../settings_shared_css.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DefaultBrowserBrowserProxy, DefaultBrowserBrowserProxyImpl, DefaultBrowserInfo} from './default_browser_browser_proxy.js';

Polymer({
  is: 'settings-default-browser-page',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private */
    isDefault_: Boolean,

    /** @private */
    isSecondaryInstall_: Boolean,

    /** @private */
    isUnknownError_: Boolean,

    /** @private */
    maySetDefaultBrowser_: Boolean,
  },

  /** @private {DefaultBrowserBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = DefaultBrowserBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'browser-default-state-changed',
        this.updateDefaultBrowserState_.bind(this));

    this.browserProxy_.requestDefaultBrowserState().then(
        this.updateDefaultBrowserState_.bind(this));
  },

  /**
   * @param {!DefaultBrowserInfo} defaultBrowserState
   * @private
   */
  updateDefaultBrowserState_(defaultBrowserState) {
    this.isDefault_ = false;
    this.isSecondaryInstall_ = false;
    this.isUnknownError_ = false;
    this.maySetDefaultBrowser_ = false;

    if (defaultBrowserState.isDefault) {
      this.isDefault_ = true;
    } else if (!defaultBrowserState.canBeDefault) {
      this.isSecondaryInstall_ = true;
    } else if (
        !defaultBrowserState.isDisabledByPolicy &&
        !defaultBrowserState.isUnknownError) {
      this.maySetDefaultBrowser_ = true;
    } else {
      this.isUnknownError_ = true;
    }
  },

  /** @private */
  onSetDefaultBrowserTap_() {
    this.browserProxy_.setAsDefaultBrowser();
  },
});

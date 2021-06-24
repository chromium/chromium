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

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DefaultBrowserBrowserProxy, DefaultBrowserBrowserProxyImpl, DefaultBrowserInfo} from './default_browser_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsDefaultBrowserPageElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsDefaultBrowserPageElement extends
    SettingsDefaultBrowserPageElementBase {
  static get is() {
    return 'settings-default-browser-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      isDefault_: Boolean,

      /** @private */
      isSecondaryInstall_: Boolean,

      /** @private */
      isUnknownError_: Boolean,

      /** @private */
      maySetDefaultBrowser_: Boolean,

    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!DefaultBrowserBrowserProxy} */
    this.browserProxy_ = DefaultBrowserBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'browser-default-state-changed',
        this.updateDefaultBrowserState_.bind(this));

    this.browserProxy_.requestDefaultBrowserState().then(
        this.updateDefaultBrowserState_.bind(this));
  }

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
  }

  /** @private */
  onSetDefaultBrowserTap_() {
    this.browserProxy_.setAsDefaultBrowser();
  }
}

customElements.define(
    SettingsDefaultBrowserPageElement.is, SettingsDefaultBrowserPageElement);

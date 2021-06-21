// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 */
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_radio_button.js';
import '../controls/extension_controlled_indicator.js';
import '../controls/settings_radio_group.js';
import './startup_urls_page.js';
import '../i18n_setup.js';
import '../settings_shared_css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NtpExtension, OnStartupBrowserProxyImpl} from './on_startup_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsOnStartupPageElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsOnStartupPageElement extends SettingsOnStartupPageElementBase {
  static get is() {
    return 'settings-on-startup-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private {?NtpExtension} */
      ntpExtension_: Object,

      /**
       * Enum values for the 'session.restore_on_startup' preference.
       * @private {!Object<string, number>}
       */
      prefValues_: {
        readOnly: true,
        type: Object,
        value: {
          CONTINUE: 1,
          OPEN_NEW_TAB: 5,
          OPEN_SPECIFIC: 4,
        },
      },

    };
  }



  /** @override */
  connectedCallback() {
    super.connectedCallback();

    const updateNtpExtension = ntpExtension => {
      // Note that |ntpExtension| is empty if there is no NTP extension.
      this.ntpExtension_ = ntpExtension;
    };
    OnStartupBrowserProxyImpl.getInstance().getNtpExtension().then(
        updateNtpExtension);
    this.addWebUIListener('update-ntp-extension', updateNtpExtension);
  }

  /**
   * @param {number} value
   * @return {string}
   * @private
   */
  getName_(value) {
    return value.toString();
  }

  /**
   * Determine whether to show the user defined startup pages.
   * @param {number} restoreOnStartup Enum value from prefValues_.
   * @return {boolean} Whether the open specific pages is selected.
   * @private
   */
  showStartupUrls_(restoreOnStartup) {
    return restoreOnStartup === this.prefValues_.OPEN_SPECIFIC;
  }
}

customElements.define(
    SettingsOnStartupPageElement.is, SettingsOnStartupPageElement);

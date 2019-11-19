// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemBehavior} from './item_behavior.js';
import {KioskApp, KioskAppSettings, KioskBrowserProxy, KioskBrowserProxyImpl} from './kiosk_browser_proxy.js';


Polymer({
  is: 'extensions-kiosk-dialog',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, ItemBehavior],

  properties: {
    /** @private {?string} */
    addAppInput_: {
      type: String,
      value: null,
    },

    /** @private {!Array<!KioskApp>} */
    apps_: Array,

    /** @private */
    bailoutDisabled_: Boolean,

    /** @private */
    canEditAutoLaunch_: Boolean,

    /** @private */
    canEditBailout_: Boolean,

    /** @private {?string} */
    errorAppId_: String,
  },

  /** @private {?KioskBrowserProxy} */
  kioskBrowserProxy_: null,

  /** @override */
  ready: function() {
    this.kioskBrowserProxy_ = KioskBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.kioskBrowserProxy_.initializeKioskAppSettings()
        .then(params => {
          this.canEditAutoLaunch_ = params.autoLaunchEnabled;
          return this.kioskBrowserProxy_.getKioskAppSettings();
        })
        .then(this.setSettings_.bind(this));

    this.addWebUIListener(
        'kiosk-app-settings-changed', this.setSettings_.bind(this));
    this.addWebUIListener('kiosk-app-updated', this.updateApp_.bind(this));
    this.addWebUIListener('kiosk-app-error', this.showError_.bind(this));

    this.$.dialog.showModal();
  },

  /**
   * @param {!KioskAppSettings} settings
   * @private
   */
  setSettings_: function(settings) {
    this.apps_ = settings.apps;
    this.bailoutDisabled_ = settings.disableBailout;
    this.canEditBailout_ = settings.hasAutoLaunchApp;
  },

  /**
   * @param {!KioskApp} app
   * @private
   */
  updateApp_: function(app) {
    const index = this.apps_.findIndex(a => a.id == app.id);
    assert(index < this.apps_.length);
    this.set('apps_.' + index, app);
  },

  /**
   * @param {string} appId
   * @private
   */
  showError_: function(appId) {
    this.errorAppId_ = appId;
  },

  /**
   * @param {string} errorMessage
   * @return {string}
   * @private
   */
  getErrorMessage_: function(errorMessage) {
    return this.errorAppId_ + ' ' + errorMessage;
  },

  /** @private */
  onAddAppTap_: function() {
    assert(this.addAppInput_);
    this.kioskBrowserProxy_.addKioskApp(this.addAppInput_);
    this.addAppInput_ = null;
  },

  /** @private */
  clearInputInvalid_: function() {
    this.errorAppId_ = null;
  },

  /**
   * @param {{model: {item: !KioskApp}}} event
   * @private
   */
  onAutoLaunchButtonTap_: function(event) {
    const app = event.model.item;
    if (app.autoLaunch) {  // If the app is originally set to
                           // auto-launch.
      this.kioskBrowserProxy_.disableKioskAutoLaunch(app.id);
    } else {
      this.kioskBrowserProxy_.enableKioskAutoLaunch(app.id);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onBailoutChanged_: function(event) {
    event.preventDefault();
    if (this.$.bailout.checked) {
      this.$['confirm-dialog'].showModal();
    } else {
      this.kioskBrowserProxy_.setDisableBailoutShortcut(false);
      this.$['confirm-dialog'].close();
    }
  },

  /** @private */
  onBailoutDialogCancelTap_: function() {
    this.$.bailout.checked = false;
    this.$['confirm-dialog'].cancel();
  },

  /** @private */
  onBailoutDialogConfirmTap_: function() {
    this.kioskBrowserProxy_.setDisableBailoutShortcut(true);
    this.$['confirm-dialog'].close();
  },

  /** @private */
  onDoneTap_: function() {
    this.$.dialog.close();
  },

  /**
   * @param {{model: {item: !KioskApp}}} event
   * @private
   */
  onDeleteAppTap_: function(event) {
    this.kioskBrowserProxy_.removeKioskApp(event.model.item.id);
  },

  /**
   * @param {boolean} autoLaunched
   * @param {string} disableStr
   * @param {string} enableStr
   * @return {string}
   * @private
   */
  getAutoLaunchButtonLabel_: function(autoLaunched, disableStr, enableStr) {
    return autoLaunched ? disableStr : enableStr;
  },

  /**
   * @param {!Event} e
   * @private
   */
  stopPropagation_: function(e) {
    e.stopPropagation();
  },
});

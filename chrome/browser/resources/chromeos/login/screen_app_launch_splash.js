// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen implementation.
 */
'use strict';

(function() {
Polymer({
  is: 'app-launch-splash-element',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'toggleNetworkConfig',
    'updateApp',
    'updateMessage',
  ],

  properties: {
    appName: {type: String, value: ''},
    appUrl: {type: String, value: ''},
    launchText: {type: String, value: ''},
  },

  ready() {
    this.initializeLoginScreen('AppLaunchSplashScreen', {
      resetAllowed: false,
    });

    var networkContainer = this.$.configNetworkContainer;
    networkContainer.addEventListener(
        'transitionend', this.onConfigNetworkTransitionend_.bind(this));

    // Ensure the transitionend event gets called after a wait time.
    // The wait time should be inline with the transition duration time
    // defined in css file. The current value in css is 1000ms. To avoid
    // the emulated transitionend firing before real one, a 1050ms
    // delay is used.
    ensureTransitionEndEvent(networkContainer, 1050);
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.KIOSK;
  },

  /** @override */
  onWindowResize() {
    if (Oobe.getInstance().currentScreen !== this)
      return;

    Oobe.getInstance().updateScreenSize(this);
  },

  onConfigNetwork_(e) {
    chrome.send('configureNetwork');
  },

  onConfigNetworkTransitionend_(e) {
    if (this.$.configNetworkContainer.classList.contains('faded'))
      this.$.configNetwork.hidden = true;
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {string} data Screen init payload.
   */
  onBeforeShow(data) {
    this.$.configNetwork.hidden = true;
    this.toggleNetworkConfig(false);
    this.updateApp(data['appInfo']);

    this.$.shortcutInfo.hidden = !data['shortcutEnabled'];

    Oobe.getInstance().solidBackground = true;
  },

  /**
   * Event handler that is invoked just before the frame is hidden.
   */
  onBeforeHide() {
    Oobe.getInstance().solidBackground = false;
  },

  /**
   * Toggles visibility of the network configuration option.
   * @param {boolean} visible Whether to show the option.
   */
  toggleNetworkConfig(visible) {
    var currVisible =
        !this.$.configNetworkContainer.classList.contains('faded');
    if (currVisible == visible)
      return;

    if (visible) {
      this.$.configNetwork.hidden = false;
      this.$.configNetworkContainer.classList.remove('faded');
    } else {
      this.$.configNetworkContainer.classList.add('faded');
    }
  },

  /**
   * Updates the app name and icon.
   * @param {Object} app Details of app being launched.
   */
  updateApp(app) {
    this.appName = app.name;
    this.appUrl = app.url;
    this.$.header.style.backgroundImage = 'url(' + app.iconURL + ')';
  },

  /**
   * Updates the message for the current launch state.
   * @param {string} message Description for current launch state.
   */
  updateMessage(message) {
    this.launchText = message;
  },
});
}());

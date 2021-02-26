// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Recommend Apps
 * screen.
 */

'use strict';

(function() {

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UIState = {
  LOADING: 'loading',
  LIST: 'list',
};

Polymer({
  is: 'recommend-apps-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'setWebview',
    'loadAppList',
  ],

  UI_STEPS: UIState,

  properties: {
    appCount_: {
      type: Number,
      value: 0,
    },

    appsSelected_: {
      type: Number,
      value: 0,
    },
  },

  initialized_: false,

  ready() {
    this.initializeLoginScreen('RecommendAppsScreen', {
      resetAllowed: true,
    });
    window.addEventListener('message', this.onMessage_.bind(this));
  },

  /**
   * Resets screen to initial state.
   * Currently is used for debugging purposes only.
   */
  reset() {
    this.setUIStep(UIState.LOADING);
    this.appCount_ = 0;
    this.appsSelected_ = 0;
  },

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.appsDialog;
  },

  defaultUIStep() {
    return UIState.LOADING;
  },

  /**
   * Initial UI State for screen
   */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  },

  setWebview(contents) {
    cr.ui.login.invokePolymerMethod(this.$.appsDialog, 'onBeforeShow');
    this.$.appView.src =
        'data:text/html;charset=utf-8,' + encodeURIComponent(contents);
  },

  /**
   * Generates the contents in the webview.
   * It is assumed that |loadAppList| is called only once after |setWebview|.
   * @suppress {missingProperties} as WebView type has no executeScript defined.
   */
  loadAppList(appList) {
    this.appCount_ = appList.length;

    const appListView = this.$.appView;
    appListView.addEventListener('contentload', () => {
      appListView.executeScript({file: 'recommend_app_list_view.js'}, () => {
        appListView.contentWindow.postMessage('initialMessage', '*');

        appList.forEach(function(app, index) {
          let generateItemScript = 'generateContents("' + app.icon + '", "' +
              app.name + '", "' + app.package_name + '");';
          const generateContents = {code: generateItemScript};
          appListView.executeScript(generateContents);
        });

        const getNumOfSelectedAppsScript = 'sendNumberOfSelectedApps();';
        appListView.executeScript({code: getNumOfSelectedAppsScript});

        this.onFullyLoaded_();
      });
    });
  },

  /**
   * Handles event when contents in the webview is generated.
   */
  onFullyLoaded_() {
    const appListView = this.$.appView;
    appListView.executeScript({code: 'getHeight();'}, function(result) {
      appListView.setAttribute('style', 'height: ' + result + 'px');
    });
    this.setUIStep(UIState.LIST);
    this.$.installButton.focus();
  },

  /**
   * Handles Skip button click.
   */
  onSkip_() {
    chrome.send('recommendAppsSkip');
  },

  /**
   * Handles Install button click.
   * @suppress {missingProperties} as WebView type has no executeScript defined.
   */
  onInstall_() {
    // Only start installation if there are apps to install.
    if (this.appsSelected_ > 0) {
      var appListView = this.$.appView;
      appListView.executeScript(
          {code: 'getSelectedPackages();'}, function(result) {
            chrome.send('recommendAppsInstall', result[0]);
          });
    }
  },

  /**
   * Handles the message sent from the WebView.
   * @param {Event} event
   */
  onMessage_(event) {
    var data =
        /** type {OobeTypes.RecommendedAppsSelectionEventData} */ event.data;
    if (data.type && (data.type === 'NUM_OF_SELECTED_APPS')) {
      this.appsSelected_ = data.numOfSelected;
    }
  },

  /**
   * Handles Select all button click.
   * @suppress {missingProperties} as WebView type has no executeScript defined.
   */
  onSelectAll_() {
    var appListView = this.$.appView;
    appListView.executeScript({code: 'selectAll();'});
  },

  canProceed_(appsSelected) {
    return appsSelected > 0;
  },
});
})();

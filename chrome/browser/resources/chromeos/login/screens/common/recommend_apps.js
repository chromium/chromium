// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Recommend Apps
 * screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
 const RecommendAppsUiState = {
  LOADING: 'loading',
  LIST: 'list',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const RecommendAppsElementBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior, MultiStepBehavior],
  Polymer.Element);

/**
 * @typedef {{
 *   appsDialog:  OobeAdaptiveDialogElement,
 *   appView:  WebView,
 *   installButton:  OobeTextButtonElement,
 * }}
 */
 RecommendAppsElementBase.$;

/**
 * @polymer
 */
class RecommendAppsElement extends RecommendAppsElementBase {

  static get is() {
    return 'recommend-apps-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      appCount_: {
        type: Number,
        value: 0,
      },

      appsSelected_: {
        type: Number,
        value: 0,
      },
    };
  }

  constructor() {
    super();
    this.initialized_ = false;
  }

  get EXTERNAL_API() {
    return ['setWebview',
            'loadAppList'];
  }

  get UI_STEPS() {
    return RecommendAppsUiState;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('RecommendAppsScreen', {
      resetAllowed: true,
    });
    window.addEventListener('message', this.onMessage_.bind(this));
  }

  /**
   * Resets screen to initial state.
   * Currently is used for debugging purposes only.
   */
  reset() {
    this.setUIStep(RecommendAppsUiState.LOADING);
    this.appCount_ = 0;
    this.appsSelected_ = 0;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.appsDialog;
  }

  defaultUIStep() {
    return RecommendAppsUiState.LOADING;
  }

  /**
   * Initial UI State for screen
   */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  setWebview(contents) {
    cr.ui.login.invokePolymerMethod(this.$.appsDialog, 'onBeforeShow');
    this.$.appView.src =
        'data:text/html;charset=utf-8,' + encodeURIComponent(contents);
  }

  /**
   * Generates the contents in the webview.
   * It is assumed that |loadAppList| is called only once after |setWebview|.
   */
  loadAppList(appList) {
    this.appCount_ = appList.length;

    const appListView = this.$.appView;
    appListView.addEventListener('contentload', () => {
      appListView.executeScript({file: 'recommend_app_list_view.js'}, () => {
        appListView.contentWindow.postMessage('initialMessage', '*');

        appList.forEach(function(app_data, index) {
          const app = /** @type {OobeTypes.RecommendedAppsExpectedAppData} */ (app_data);
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
  }

  /**
   * Handles event when contents in the webview is generated.
   */
  onFullyLoaded_() {
    const appListView = this.$.appView;
    appListView.executeScript({code: 'getHeight();'}, function(result) {
      appListView.setAttribute('style', 'height: ' + result + 'px');
    });
    this.setUIStep(RecommendAppsUiState.LIST);
    this.$.installButton.focus();
  }

  /**
   * Handles Skip button click.
   */
  onSkip_() {
    chrome.send('recommendAppsSkip');
  }

  /**
   * Handles Install button click.
   */
  onInstall_() {
    // Only start installation if there are apps to install.
    if (this.appsSelected_ > 0) {
      let appListView = this.$.appView;
      appListView.executeScript(
          {code: 'getSelectedPackages();'}, function(result) {
            chrome.send('recommendAppsInstall', [result[0]]);
          });
    }
  }

  /**
   * Handles the message sent from the WebView.
   * @param {Event} event
   */
  onMessage_(event) {
    let data =
        /** @type {OobeTypes.RecommendedAppsSelectionEventData} */ (event.data);
    if (data.type && (data.type === 'NUM_OF_SELECTED_APPS')) {
      this.appsSelected_ = data.numOfSelected;
    }
  }

  /**
   * Handles Select all button click.
   */
  onSelectAll_() {
    let appListView = this.$.appView;
    appListView.executeScript({code: 'selectAll();'});
  }

  canProceed_(appsSelected) {
    return appsSelected > 0;
  }
}

customElements.define(RecommendAppsElement.is, RecommendAppsElement);

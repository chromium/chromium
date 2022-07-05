// Copyright 2018 The Chromium Authors. All rights reserved.
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

const BLANK_PAGE_URL = 'about:blank';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const RecommendAppsElementBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior, MultiStepBehavior],
  Polymer.Element);

/**
 * @typedef {{
 *   appsDialog:  OobeAdaptiveDialogElement,
 *   appView:  WebView,
 *   appsList: OobeAppsList,
 *   installButton:  OobeTextButton,
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

      appList_: {
        type: Array,
        value: [],
      },

      /**
       * If new version of screen available.
       * @private
       */
      isOobeNewRecommendAppsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOobeNewRecommendAppsEnabled');
        },
        readOnly: true,
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
    this.initializeLoginScreen('RecommendAppsScreen');
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

  onBeforeHide() {
    if (this.isOobeNewRecommendAppsEnabled_) {
      this.appList_ = [];
      return;
    }
    const appListView = this.shadowRoot.querySelector('#appView');
    appListView.src = BLANK_PAGE_URL;
  }

  setWebview(contents) {
    cr.ui.login.invokePolymerMethod(this.$.appsDialog, 'onBeforeShow');
    // Can't use this.$.appView here as the element is in a <dom-if>.
    const appListView = this.shadowRoot.querySelector('#appView');
    appListView.src =
        'data:text/html;charset=utf-8,' + encodeURIComponent(contents);
  }

  /**
   * Generates the contents in the webview.
   * It is assumed that |loadAppList| is called only once after |setWebview|.
   */
  loadAppList(appList) {
    if (this.isOobeNewRecommendAppsEnabled_) {
      const recommendAppsContainsAdsStr = this.i18n('recommendAppsContainsAds');
      const recommendAppsInAppPurchasesStr =
          this.i18n('recommendAppsInAppPurchases');
      const recommendAppsWasInstalledStr =
          this.i18n('recommendAppsWasInstalled');
      this.appList_ = appList.map(app => {
        const tagList = [app.category];
        if (app.contains_ads) {
          tagList.push(recommendAppsContainsAdsStr);
        }
        if (app.in_app_purchases) {
          tagList.push(recommendAppsInAppPurchasesStr);
        }
        if (app.was_installed) {
          tagList.push(recommendAppsWasInstalledStr);
        }
        if (app.content_rating) {
          tagList.push(app.content_rating);
        }
        return {
          title: app.title,
          icon_url: app.icon_url,
          tags: tagList,
          description: app.description,
          package_name: app.package_name,
          checked: false,
        };
      });
      return;
    }
    this.appCount_ = appList.length;

    // Can't use this.$.appView here as the element is in a <dom-if>.
    const appListView = this.shadowRoot.querySelector('#appView');
    appListView.addEventListener('contentload', () => {
      appListView.executeScript(
          {file: 'recommend_app_list_view.js'}, () => {
            appListView.contentWindow.postMessage('initialMessage', '*');

            appList.forEach(function(app_data, index) {
              const app =
                  /** @type {OobeTypes.RecommendedAppsExpectedAppData} */ (
                      app_data);
              const generateItemScript = 'generateContents("' + app.icon +
                  '", "' + app.name + '", "' + app.package_name + '");';
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
    this.setUIStep(RecommendAppsUiState.LIST);
    if (this.isOobeNewRecommendAppsEnabled_) {
      this.shadowRoot.querySelector('#appsList').focus();
      return;
    }
    this.shadowRoot.querySelector('#selectAllLink').focus();
  }

  /**
   * Handles Skip button click.
   */
  onSkip_() {
    this.userActed('recommendAppsSkip');
  }

  /**
   * Handles Install button click.
   */
  onInstall_() {
    // Only start installation if there are apps to install.
    if (this.appsSelected_ > 0) {
      if (this.isOobeNewRecommendAppsEnabled_) {
        // Can't use this.$.appsList here as the element is in a <dom-if>.
        const appsList = this.shadowRoot.querySelector('#appsList');
        const packageNames = appsList.getSelectedApps();
        this.userActed(['recommendAppsInstall', packageNames]);
        return;
      }
      // Can't use this.$.appView here as the element is in a <dom-if>.
      const appListView = this.shadowRoot.querySelector('#appView');
      appListView.executeScript({code: 'getSelectedPackages();'}, (result) => {
        this.userActed(['recommendAppsInstall', result[0]]);
      });
    }
  }

  /**
   * Handles the message sent from the WebView.
   * @param {Event} event
   */
  onMessage_(event) {
    const data =
        /** @type {OobeTypes.RecommendedAppsSelectionEventData} */ (event.data);
    if (data.type && (data.type === 'NUM_OF_SELECTED_APPS')) {
      this.appsSelected_ = data.numOfSelected;
    }
  }

  /**
   * Handles Select all button click.
   */
  onSelectAll_() {
    // Can't use this.$.appView here as the element is in a <dom-if>.
    const appListView = this.shadowRoot.querySelector('#appView');
    if (!this.isOobeNewRecommendAppsEnabled_) {
      appListView.executeScript({code: 'selectAll();'});
      return;
    }
  }

  canProceed_(appsSelected) {
    return appsSelected > 0;
  }
}

customElements.define(RecommendAppsElement.is, RecommendAppsElement);

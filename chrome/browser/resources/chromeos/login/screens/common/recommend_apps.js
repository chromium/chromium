// Copyright 2018 The Chromium Authors
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
 *   appsDialog:  OobeAdaptiveDialog,
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
      appsSelected_: {
        type: Number,
        value: 0,
      },

      appList_: {
        type: Array,
        value: [],
      },
    };
  }

  constructor() {
    super();
    this.initialized_ = false;
  }

  get EXTERNAL_API() {
    return ['loadAppList'];
  }

  get UI_STEPS() {
    return RecommendAppsUiState;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('RecommendAppsScreen');
  }

  /**
   * Resets screen to initial state.
   * Currently is used for debugging purposes only.
   */
  reset() {
    this.setUIStep(RecommendAppsUiState.LOADING);
    this.appsSelected_ = 0;
    this.appList_ = [];
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
    this.appList_ = [];
    return;
  }

  /**
   * Generates the contents in the webview.
   */
  loadAppList(appList) {
    const recommendAppsContainsAdsStr = this.i18n('recommendAppsContainsAds');
    const recommendAppsInAppPurchasesStr =
        this.i18n('recommendAppsInAppPurchases');
    const recommendAppsWasInstalledStr = this.i18n('recommendAppsWasInstalled');
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

  /**
   * Handles event when contents in the webview is generated.
   */
  onFullyLoaded_() {
    this.setUIStep(RecommendAppsUiState.LIST);
    this.shadowRoot.querySelector('#appsList').focus();
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
    // Button should be disabled if nothing is selected.
    assert(this.appsSelected_ > 0);
    // Can't use this.$.appsList here as the element is in a <dom-if>.
    const appsList = this.shadowRoot.querySelector('#appsList');
    const packageNames = appsList.getSelectedApps();
    this.userActed(['recommendAppsInstall', packageNames]);
  }

  canProceed_(appsSelected) {
    return appsSelected > 0;
  }
}

customElements.define(RecommendAppsElement.is, RecommendAppsElement);

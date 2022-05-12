// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './dom_switch.js';
import './pwa_detail_view.js';
import './arc_detail_view.js';
import './chrome_app_detail_view.js';
import './plugin_vm_page/plugin_vm_detail_view.js';
import './borealis_page/borealis_detail_view.js';
import '../../../settings_shared_css.js';

import {AppManagementUserAction, AppType} from '//resources/cr_components/app_management/constants.js';
import {getSelectedApp, recordAppManagementUserAction} from '//resources/cr_components/app_management/util.js';
import {assertNotReached} from '//resources/js/assert.m.js';
import {html, microTask, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../../router.js';
import {routes} from '../../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../../route_observer_behavior.js';

import {updateSelectedAppId} from './actions.js';
import {AppManagementStoreClient, AppManagementStoreClientInterface} from './store_client.js';
import {openMainPage} from './util.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const AppManagementAppDetailViewElementBase = mixinBehaviors(
    [AppManagementStoreClient, RouteObserverBehavior], PolymerElement);

/** @polymer */
class AppManagementAppDetailViewElement extends
    AppManagementAppDetailViewElementBase {
  static get is() {
    return 'app-management-app-detail-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {App}
       * @private
       */
      app_: {
        type: Object,
      },

      /**
       * @type {AppMap}
       * @private
       */
      apps_: {
        type: Object,
        observer: 'appsChanged_',
      },

      /**
       * @type {string}
       * @private
       */
      selectedAppId_: {
        type: String,
        observer: 'selectedAppIdChanged_',
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.watch('apps_', state => state.apps);
    this.watch('selectedAppId_', state => state.selectedAppId);
    this.updateFromStore();
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    this.dispatch(updateSelectedAppId(null));
  }

  /**
   * Updates selected app ID based on the URL query params.
   *
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(currentRoute, prevRoute) {
    if (currentRoute !== routes.APP_MANAGEMENT_DETAIL) {
      return;
    }

    if (this.selectedAppNotFound_()) {
      microTask.run(() => {
        openMainPage();
      });
      return;
    }

    const appId = Router.getInstance().getQueryParameters().get('id');

    this.dispatch(updateSelectedAppId(appId));
  }

  /**
   * @param {?App} app
   * @return {?string}
   * @private
   */
  getSelectedRouteId_(app) {
    if (!app) {
      return null;
    }

    const selectedAppType = app.type;
    switch (selectedAppType) {
      case (AppType.kWeb):
        return 'pwa-detail-view';
      case (AppType.kChromeApp):
      case (AppType.kStandaloneBrowser):
      case (AppType.kStandaloneBrowserChromeApp):
        // TODO(https://crbug.com/1225848): Figure out appropriate behavior for
        // Lacros-hosted chrome-apps.
        return 'chrome-app-detail-view';
      case (AppType.kArc):
        return 'arc-detail-view';
      case (AppType.kPluginVm):
        return 'plugin-vm-detail-view';
      case (AppType.kBorealis):
        return 'borealis-detail-view';
      default:
        assertNotReached();
    }
  }

  /** @private */
  selectedAppIdChanged_(appId) {
    if (appId && this.app_) {
      recordAppManagementUserAction(
          this.app_.type, AppManagementUserAction.VIEW_OPENED);
    }
  }

  /** @private */
  appsChanged_() {
    if (Router.getInstance().getCurrentRoute() ===
            routes.APP_MANAGEMENT_DETAIL &&
        this.selectedAppNotFound_()) {
      microTask.run(() => {
        openMainPage();
      });
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  selectedAppNotFound_() {
    const appId = /** @type {string} */ (
        Router.getInstance().getQueryParameters().get('id'));
    return this.apps_ && !this.apps_[appId];
  }
}

customElements.define(
    AppManagementAppDetailViewElement.is, AppManagementAppDetailViewElement);

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './dom_switch.js';
import './pwa_detail_view.js';
import './arc_detail_view.js';
import './chrome_app_detail_view.js';
import './plugin_vm_page/plugin_vm_detail_view.js';
import './borealis_page/borealis_detail_view.js';
import '../../settings_shared.css.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction, AppType} from 'chrome://resources/cr_components/app_management/constants.js';
import {getSelectedApp, recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../../assert_extras.js';
import {updateSelectedAppId} from '../../common/app_management/actions.js';
import {AppMap} from '../../common/app_management/store.js';
import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';
import {RouteObserverMixin} from '../../common/route_observer_mixin.js';
import {PrefsState} from '../../common/types.js';
import {Route, Router, routes} from '../../router.js';

import {getTemplate} from './app_detail_view.html.js';
import {openMainPage} from './util.js';

const AppManagementAppDetailViewElementBase =
    AppManagementStoreMixin(RouteObserverMixin(PolymerElement));

export class AppManagementAppDetailViewElement extends
    AppManagementAppDetailViewElementBase {
  static get is() {
    return 'app-management-app-detail-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
      app_: {
        type: Object,
      },

      apps_: {
        type: Object,
        observer: 'appsChanged_',
      },

      selectedAppId_: {
        type: String,
        observer: 'selectedAppIdChanged_',
      },
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  private app_: App;
  private apps_: AppMap;
  private selectedAppId_: string;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.watch('apps_', state => state.apps);
    this.watch('selectedAppId_', state => state.selectedAppId);
    this.updateFromStore();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.dispatch(updateSelectedAppId(null));
  }

  /**
   * Updates selected app ID based on the URL query params.
   */
  override currentRouteChanged(currentRoute: Route): void {
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

  private getSelectedRouteId_(app: App|null): string|null {
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
        // TODO(crbug.com/40188614): Figure out appropriate behavior for
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

  private selectedAppIdChanged_(appId: string): void {
    if (appId && this.app_) {
      recordAppManagementUserAction(
          this.app_.type, AppManagementUserAction.VIEW_OPENED);
    }
  }

  private appsChanged_(): void {
    if (Router.getInstance().currentRoute === routes.APP_MANAGEMENT_DETAIL &&
        this.selectedAppNotFound_()) {
      microTask.run(() => {
        openMainPage();
      });
    }
  }

  private selectedAppNotFound_(): boolean {
    const appId =
        castExists(Router.getInstance().getQueryParameters().get('id'));
    return Boolean(this.apps_) && !this.apps_[appId];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-app-detail-view': AppManagementAppDetailViewElement;
  }
}

customElements.define(
    AppManagementAppDetailViewElement.is, AppManagementAppDetailViewElement);

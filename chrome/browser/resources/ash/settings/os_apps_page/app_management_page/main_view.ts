// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_item.js';
import './app_management_cros_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {alphabeticalSort} from 'chrome://resources/cr_components/app_management/util.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStore, AppMap} from '../../common/app_management/store.js';
import {AppManagementStoreMixin} from '../../common/app_management/store_mixin.js';
import {RouteObserverMixin} from '../../common/route_observer_mixin.js';
import {Route, routes} from '../../router.js';

import {getTemplate} from './main_view.html.js';

const AppManagementMainViewElementBase =
    AppManagementStoreMixin(RouteObserverMixin(PolymerElement));

export class AppManagementMainViewElement extends
    AppManagementMainViewElementBase {
  static get is() {
    return 'app-management-main-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchTerm: {
        type: String,
      },

      apps_: {
        type: Object,
      },

      appList_: {
        type: Array,
        value: () => [],
        computed: 'computeAppList_(apps_, searchTerm)',
      },
    };
  }

  searchTerm: string;
  private appList_: App[];
  private apps_: AppMap|undefined;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  }

  override currentRouteChanged(route: Route): void {
    if (route === routes.APP_MANAGEMENT) {
      const appId = AppManagementStore.getInstance().data.selectedAppId;

      // Expect this to be false the first time the "Manage your apps" page
      // is requested as no app has been selected yet.
      if (appId) {
        const button = this.shadowRoot!.querySelector<CrIconButtonElement>(
            `#app-subpage-button-${appId}`);
        if (button) {
          focusWithoutInk(button);
        }
      }
    }
  }

  private isAppListEmpty_(appList: App[]): boolean {
    return appList.length === 0;
  }

  private computeAppList_(apps: AppMap|undefined, searchTerm: string): App[] {
    if (!apps) {
      return [];
    }

    // This is calculated locally as once the user leaves this page the state
    // should reset.
    const appArray = Object.values(apps);

    let filteredApps: App[];
    if (searchTerm) {
      const lowerCaseSearchTerm = searchTerm.toLowerCase();
      filteredApps = appArray.filter(app => {
        assert(app.title);
        return app.title.toLowerCase().includes(lowerCaseSearchTerm);
      });
    } else {
      filteredApps = appArray;
    }

    filteredApps.sort((a, b) => {
      assert(a.title);
      assert(b.title);
      return alphabeticalSort(a.title, b.title);
    });

    return filteredApps;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-main-view': AppManagementMainViewElement;
  }
}

customElements.define(
    AppManagementMainViewElement.is, AppManagementMainViewElement);

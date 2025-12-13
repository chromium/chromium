// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './people_page.js';
import '../settings_shared.css.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './people_page_index.html.js';


export interface SettingsPeoplePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsPeoplePageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsPeoplePageIndexElement extends
    SettingsPeoplePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-people-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      routes_: {
        type: Object,
        value: () => routes,
      },

      // <if expr="not is_chromeos">
      replaceSyncPromosWithSignInPromos_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'),
      },
      // </if>
    };
  }

  declare prefs: {[key: string]: any};
  declare private routes_: SettingsRoutes;

  // <if expr="not is_chromeos">
  declare private replaceSyncPromosWithSignInPromos_: boolean;
  // </if>

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.PEOPLE:
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        case routes.BASIC:
          // Switch back to the default views in case they are part of search
          // results.
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        case routes.SYNC:
          this.$.viewManager.switchView('sync', 'no-animation', 'no-animation');
          break;
        case routes.SYNC_ADVANCED:
          this.$.viewManager.switchView(
              'syncControls', 'no-animation', 'no-animation');
          break;
        // <if expr="not is_chromeos">
        case routes.IMPORT_DATA:
        case routes.SIGN_OUT:
          // Switch to settings-people-page since these dialogs reside
          // there, otherwise they will not be visible even if open.
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        case routes.ACCOUNT:
          assert(this.replaceSyncPromosWithSignInPromos_);
          this.$.viewManager.switchView(
              'account', 'no-animation', 'no-animation');
          break;
        case routes.GOOGLE_SERVICES:
          assert(this.replaceSyncPromosWithSignInPromos_);
          this.$.viewManager.switchView(
              'googleServices', 'no-animation', 'no-animation');
          break;
        case routes.MANAGE_PROFILE:
          this.$.viewManager.switchView(
              'manageProfile', 'no-animation', 'no-animation');
          break;
        // </if>
        default:
          // Nothing to do. Other parent elements are responsible for updating
          // the displayed contents.
          break;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-people-page-index': SettingsPeoplePageIndexElement;
  }
}

customElements.define(
    SettingsPeoplePageIndexElement.is, SettingsPeoplePageIndexElement);

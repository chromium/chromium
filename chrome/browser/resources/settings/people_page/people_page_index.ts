// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './people_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './people_page_index.css.js';
import {getHtml} from './people_page_index.html.js';


export interface SettingsPeoplePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export type PeoplePageIndexElement = SettingsPeoplePageIndexElement;

const SettingsPeoplePageIndexElementBase =
    SearchableViewContainerMixinLit(RouteObserverMixinLit(CrLitElement));

export class SettingsPeoplePageIndexElement extends
    SettingsPeoplePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-people-page-index';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      routes_: {type: Object},
      replaceSyncPromosWithSignInPromos_: {type: Boolean},
    };
  }

  protected accessor routes_: SettingsRoutes = routes;
  protected accessor replaceSyncPromosWithSignInPromos_: boolean =
      loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos');

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
        // <if expr="not is_chromeos">
        case routes.IMPORT_DATA:
        case routes.SIGN_OUT:
          // Switch to settings-people-page since these dialogs reside
          // there, otherwise they will not be visible even if open.
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
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

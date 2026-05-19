// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './a11y_page.js';
import '../settings_shared.css.js';
// <if expr="is_linux">
import './captions_page.js';

// </if>

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './a11y_page_index.html.js';


export interface SettingsA11yPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsA11yPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsA11yPageIndexElement extends
    SettingsA11yPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-a11y-page-index';
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
    };
  }

  declare prefs: {[key: string]: any};
  declare private routes_: SettingsRoutes;

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.ACCESSIBILITY:
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        // <if expr="is_linux">
        case routes.CAPTIONS:
          this.$.viewManager.switchView(
              'captions', 'no-animation', 'no-animation');
          break;
        // </if>
        case routes.BASIC:
          // Switch back to the default views in case they are part of search
          // results.
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
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
    'settings-a11y-page-index': SettingsA11yPageIndexElement;
  }
}

customElements.define(
    SettingsA11yPageIndexElement.is, SettingsA11yPageIndexElement);

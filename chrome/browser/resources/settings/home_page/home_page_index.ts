// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './home_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import type {Route, SettingsRoutes} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './home_page_index.css.js';
import {getHtml} from './home_page_index.html.js';

export interface SettingsHomePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsHomePageIndexElementBase =
    SearchableViewContainerMixinLit(RouteObserverMixinLit(CrLitElement));

export class SettingsHomePageIndexElement extends
    SettingsHomePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-home-page-index';
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
    };
  }

  protected accessor routes_: SettingsRoutes = routes;

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.BASIC:
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
    'settings-home-page-index': SettingsHomePageIndexElement;
  }
}

customElements.define(
    SettingsHomePageIndexElement.is, SettingsHomePageIndexElement);

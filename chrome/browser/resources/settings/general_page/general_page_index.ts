// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '../downloads_page/downloads_page.js';
import '../on_startup_page/on_startup_page.js';
// <if expr="not is_chromeos">
import '../system_page/system_page.js';

// </if>

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './general_page_index.css.js';
import {getHtml} from './general_page_index.html.js';

export interface SettingsGeneralPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsGeneralPageIndexElementBase =
    SearchableViewContainerMixinLit(RouteObserverMixinLit(CrLitElement));

export class SettingsGeneralPageIndexElement extends
    SettingsGeneralPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-general-page-index';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private showDefaultViews_() {
    this.$.viewManager.switchViews(
        [
          'onStartup',
          'downloads',
          // <if expr="not is_chromeos">
          'system',
          // </if>
        ],
        'no-animation', 'no-animation');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.GENERAL:
          this.showDefaultViews_();
          break;
        case routes.BASIC:
          // Switch back to the default views in case they are part of search
          // results.
          this.showDefaultViews_();
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
    'settings-general-page-index': SettingsGeneralPageIndexElement;
  }
}

customElements.define(
    SettingsGeneralPageIndexElement.is, SettingsGeneralPageIndexElement);

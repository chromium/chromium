// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './site_shortcuts_page.js';
import './feature_shortcuts_page.js';
import './keyboard_shortcut_page.js';
import './search_page.js';
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

import {getTemplate} from './search_page_index.html.js';


export interface SettingsSearchPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsSearchPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsSearchPageIndexElement extends
    SettingsSearchPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-search-page-index';
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

      searchSettingsUpdateEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchSettingsUpdate'),
      },
    };
  }

  declare prefs: Record<string, unknown>;
  declare private routes_: SettingsRoutes;
  declare private searchSettingsUpdateEnabled_: boolean;

  private showDefaultViews_() {
    const defaultViews: string[] = ['parent'];

    if (this.searchSettingsUpdateEnabled_) {
      defaultViews.push(
          'siteShortcuts', 'featureShortcuts', 'keyboardShortcut');
    }

    this.$.viewManager.switchViews(
        defaultViews, 'no-animation', 'no-animation');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.SEARCH:
          this.showDefaultViews_();
          break;
        case routes.SEARCH_ENGINES:
          assert(!this.searchSettingsUpdateEnabled_);
          this.$.viewManager.switchView(
              'searchEngines', 'no-animation', 'no-animation');
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
    'settings-search-page-index': SettingsSearchPageIndexElement;
  }
}

customElements.define(
    SettingsSearchPageIndexElement.is, SettingsSearchPageIndexElement);

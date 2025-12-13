// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';
import './languages_page.js';
import './spell_check_page.js';
import './translate_page.js';
// <if expr="not is_macosx">
import './edit_dictionary_page.js';

// </if>

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './languages_page_index.html.js';
import type {LanguagesModel} from './languages_types.js';


export interface SettingsLanguagesPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsLanguagesPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsLanguagesPageIndexElement extends
    SettingsLanguagesPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-languages-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,
      languages: Object,

      routes_: {
        type: Object,
        value: () => routes,
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare languages?: LanguagesModel;
  declare private routes_: SettingsRoutes;

  private showDefaultViews_() {
    this.$.viewManager.switchViews(
        ['languages', 'spellCheck', 'translate'], 'no-animation',
        'no-animation');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.LANGUAGES:
          this.showDefaultViews_();
          break;
        // <if expr="not is_macosx">
        case routes.EDIT_DICTIONARY:
          this.$.viewManager.switchViews(
              ['editDictionary'], 'no-animation', 'no-animation');
          break;
        // </if>
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
    'settings-languages-page-index': SettingsLanguagesPageIndexElement;
  }
}

customElements.define(
    SettingsLanguagesPageIndexElement.is, SettingsLanguagesPageIndexElement);

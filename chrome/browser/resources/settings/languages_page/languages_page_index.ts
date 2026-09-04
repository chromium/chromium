// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './languages_page.js';
import './spell_check_page.js';
import './translate_page.js';
// <if expr="not is_macosx">
import './edit_dictionary_page.js';

// </if>

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './languages_page_index.css.js';
import {getHtml} from './languages_page_index.html.js';

export interface SettingsLanguagesPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export type LanguagesPageIndexElement = SettingsLanguagesPageIndexElement;

const SettingsLanguagesPageIndexElementBase =
    SearchableViewContainerMixinLit(RouteObserverMixinLit(CrLitElement));

export class SettingsLanguagesPageIndexElement extends
    SettingsLanguagesPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-languages-page-index';
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

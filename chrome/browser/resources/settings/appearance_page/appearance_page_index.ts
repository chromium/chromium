// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './appearance_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixinLit} from '../settings_page/searchable_view_container_mixin_lit.js';

import {getCss} from './appearance_page_index.css.js';
import {getHtml} from './appearance_page_index.html.js';


export interface SettingsAppearancePageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsAppearancePageIndexElementBase =
    SearchableViewContainerMixinLit(CrLitElement);

export class SettingsAppearancePageIndexElement extends
    SettingsAppearancePageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-appearance-page-index';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      routes_: {
        type: Object,
      },
    };
  }

  protected accessor routes_: SettingsRoutes = routes;

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.APPEARANCE:
          this.$.viewManager.switchView(
              'parent', 'no-animation', 'no-animation');
          break;
        case routes.FONTS:
          this.$.viewManager.switchView(
              'fonts', 'no-animation', 'no-animation');
          break;
        case routes.BASIC:
          // Switch back to the default view in case they are part of search
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
    'settings-appearance-page-index': SettingsAppearancePageIndexElement;
  }
}

customElements.define(
    SettingsAppearancePageIndexElement.is, SettingsAppearancePageIndexElement);

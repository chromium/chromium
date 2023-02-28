// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-chromevox-subpage' is the accessibility settings subpage for
 * ChromeVox settings.
 */

import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './chromevox_subpage.html.js';

const SettingsChromeVoxSubpageElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

class SettingsChromeVoxSubpageElement extends
    SettingsChromeVoxSubpageElementBase {
  static get is() {
    return 'settings-chromevox-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  static get observers() {
    return [];
  }

  private route_: Route;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_CHROMEVOX;
  }

  /**
   * Note: Overrides RouteOriginMixin implementation.
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route_) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsChromeVoxSubpageElement.is]: SettingsChromeVoxSubpageElement;
  }
}

customElements.define(
    SettingsChromeVoxSubpageElement.is, SettingsChromeVoxSubpageElement);

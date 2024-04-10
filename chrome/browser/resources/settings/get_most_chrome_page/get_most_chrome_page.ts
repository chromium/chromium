// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-get-most-chrome-page' is the settings page information about how
 * to get the most out of Chrome.
 */

import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

import {getTemplate} from './get_most_chrome_page.html.js';

const SettingsGetMostChromePageElementBase = RouteObserverMixin(PolymerElement);

export class SettingsGetMostChromePageElement extends
    SettingsGetMostChromePageElementBase {
  static get is() {
    return 'settings-get-most-chrome-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expandedFirst_: Boolean,
      expandedSecond_: Boolean,
      expandedThird_: Boolean,
    };
  }

  private expandedFirst_: boolean;
  private expandedSecond_: boolean;
  private expandedThird_: boolean;

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === Router.getInstance().getRoutes().GET_MOST_CHROME) {
      HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
          TrustSafetyInteraction.OPENED_GET_MOST_CHROME);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-get-most-chrome-page': SettingsGetMostChromePageElement;
  }
}

customElements.define(
    SettingsGetMostChromePageElement.is, SettingsGetMostChromePageElement);

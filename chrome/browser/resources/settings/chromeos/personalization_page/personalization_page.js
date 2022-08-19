// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsPersonalizationPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class SettingsPersonalizationPageElement extends
    SettingsPersonalizationPageElementBase {
  static get is() {
    return 'settings-personalization-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: Object,

      /** @private */
      isPersonalizationHubEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isPersonalizationHubEnabled');
        },
        readOnly: true,
      },

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          return map;
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kOpenWallpaper]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!PersonalizationHubBrowserProxy} */
    this.personalizationHubBrowserProxy_ =
        PersonalizationHubBrowserProxyImpl.getInstance();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.PERSONALIZATION) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  openPersonalizationHub_() {
    this.personalizationHubBrowserProxy_.openPersonalizationHub();
  }
}

customElements.define(
    SettingsPersonalizationPageElement.is, SettingsPersonalizationPageElement);

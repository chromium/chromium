// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
import '../ambient_mode_page/ambient_mode_page.js';
import '../ambient_mode_page/ambient_mode_photos_page.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import './change_picture.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_hub_browser_proxy.js';
import {WallpaperBrowserProxy, WallpaperBrowserProxyImpl} from './wallpaper_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsPersonalizationPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, I18nBehavior, RouteObserverBehavior], PolymerElement);

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
      showWallpaperRow_: {type: Boolean, value: true},

      /** @private */
      isWallpaperPolicyControlled_: {type: Boolean, value: true},

      /** @private */
      isAmbientModeEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAmbientModeEnabled');
        },
        readOnly: true,
      },

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
          if (routes.CHANGE_PICTURE) {
            map.set(routes.CHANGE_PICTURE.path, '#changePictureRow');
          } else if (routes.AMBIENT_MODE) {
            map.set(routes.AMBIENT_MODE.path, '#ambientModeRow');
          }

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

    /** @private {!WallpaperBrowserProxy} */
    this.wallpaperBrowserProxy_ = WallpaperBrowserProxyImpl.getInstance();

    /** @private {!PersonalizationHubBrowserProxy} */
    this.personalizationHubBrowserProxy_ =
        PersonalizationHubBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.wallpaperBrowserProxy_.isWallpaperSettingVisible().then(
        isWallpaperSettingVisible => {
          this.showWallpaperRow_ = isWallpaperSettingVisible;
        });
    this.wallpaperBrowserProxy_.isWallpaperPolicyControlled().then(
        isPolicyControlled => {
          this.isWallpaperPolicyControlled_ = isPolicyControlled;
        });
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

  /**
   * @private
   */
  openWallpaperManager_() {
    this.wallpaperBrowserProxy_.openWallpaperManager();
  }

  /** @private */
  openPersonalizationHub_() {
    this.personalizationHubBrowserProxy_.openPersonalizationHub();
  }

  /** @private */
  navigateToChangePicture_() {
    Router.getInstance().navigateTo(routes.CHANGE_PICTURE);
  }

  /** @private */
  navigateToAmbientMode_() {
    Router.getInstance().navigateTo(routes.AMBIENT_MODE);
  }

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAmbientModeRowSubLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'ambientModeEnabled' : 'ambientModeDisabled');
  }
}

customElements.define(
    SettingsPersonalizationPageElement.is, SettingsPersonalizationPageElement);

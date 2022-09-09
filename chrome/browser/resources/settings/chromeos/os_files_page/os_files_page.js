// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import './smb_shares_page.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsFilesPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class OsSettingsFilesPageElement extends OsSettingsFilesPageElementBase {
  static get is() {
    return 'os-settings-files-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SMB_SHARES) {
            map.set(routes.SMB_SHARES.path, '#smbShares');
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
        value: () => new Set([Setting.kGoogleDriveConnection]),
      },
    };
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.FILES) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onTapSmbShares_() {
    Router.getInstance().navigateTo(routes.SMB_SHARES);
  }
}

customElements.define(
    OsSettingsFilesPageElement.is, OsSettingsFilesPageElement);

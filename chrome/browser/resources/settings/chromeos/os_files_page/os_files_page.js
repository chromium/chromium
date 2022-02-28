// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 *
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import './smb_shares_page.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-files-page',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
  ],

  properties: {
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
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kGoogleDriveConnection]),
    },
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.FILES) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onTapSmbShares_() {
    Router.getInstance().navigateTo(routes.SMB_SHARES);
  },
});

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-smart-privacy-page' contains smart privacy settings.
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../controls/extension_controlled_indicator.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const SettingsSmartPrivacyPageBase = mixinBehaviors(
    [DeepLinkingBehavior, PrefsBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class SettingsSmartPrivacyPage extends SettingsSmartPrivacyPageBase {
  static get is() {
    return 'settings-smart-privacy-page';
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

      /**
       * Whether the smart privacy page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether or not quick dim is enabled.
       * @private {boolean}
       */
      isQuickDimEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isQuickDimEnabled');
        },
      },

      /**
       * Whether or not snooping protection is enabled.
       * @private {boolean}
       */
      isSnoopingProtectionEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSnoopingProtectionEnabled');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kQuickDim,
          chromeos.settings.mojom.Setting.kSnoopingProtection,
        ]),
      },
    };
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== routes.SMART_PRIVACY) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns the image source based on whether the smart privacy page is being
   * rendered in dark mode.
   * @returns {string}
   * @private
   */
  getImageSource_() {
    return this.isDarkModeActive_ ?
        'chrome://os-settings/images/smart_privacy_dark.svg' :
        'chrome://os-settings/images/smart_privacy.svg';
  }
}

customElements.define(SettingsSmartPrivacyPage.is, SettingsSmartPrivacyPage);

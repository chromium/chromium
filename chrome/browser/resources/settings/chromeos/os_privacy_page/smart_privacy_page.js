// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-smart-privacy-page' contains smart privacy settings.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../../controls/extension_controlled_indicator.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * The values that the quick lock slider can have, in ms.
 * @const {!Array<number>}
 */
const QUICK_LOCK_DELAY_MS = [
  30000,
  60000,
  120000,
  180000,
];

/**
 * Formatter for displaying duration text for the slider of quick dim
 * delay.
 * @const {Object}
 */
const secondsFormatter = new Intl.NumberFormat(
    window.navigator.language,
    {style: 'unit', unit: 'second', unitDisplay: 'narrow'});

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
       * Text that shows when moving the quick dim delay slider.
       * @private {!Array<!SliderTick>}
       */
      smartPrivacyQuickLockRangeMs_: {
        readOnly: true,
        type: Array,
        value() {
          return QUICK_LOCK_DELAY_MS.map(
              x => ({label: secondsFormatter.format(x / 1000), value: x}));
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kQuickDim,
          Setting.kSnoopingProtection,
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

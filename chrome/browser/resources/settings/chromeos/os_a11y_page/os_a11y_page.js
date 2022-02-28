// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-a11y-page' is the small section of advanced settings containing
 * a subpage with Accessibility settings for ChromeOS.
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../a11y_page/captions_subpage.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import './manage_a11y_page.js';
import './switch_access_subpage.js';
import './tts_subpage.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page_browser_proxy.js';

/**
 * TODO(dpapad): Remove this when os_a11y_page.js is migrated to TypeScript.
 * @interface
 */
class SettingsCaptionsElement {
  /** @return {SettingsToggleButtonElement} */
  getLiveCaptionToggle() {}
}

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-a11y-page',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether to show accessibility labels settings.
     */
    showAccessibilityLabelsSetting_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.MANAGE_ACCESSIBILITY) {
          map.set(routes.MANAGE_ACCESSIBILITY.path, '#subpage-trigger');
        }
        return map;
      },
    },

    /**
     * Whether the user is in kiosk mode.
     * @private
     */
    isKioskModeActive_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isKioskModeActive');
      }
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kA11yQuickSettings,
        chromeos.settings.mojom.Setting.kGetImageDescriptionsFromGoogle,
        chromeos.settings.mojom.Setting.kLiveCaption,
      ]),
    },
  },

  /** @private {?OsA11yPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = OsA11yPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'screen-reader-state-changed',
        hasScreenReader => this.onScreenReaderStateChanged_(hasScreenReader));

    // Enables javascript and gets the screen reader state.
    this.browserProxy_.a11yPageReady();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId === chromeos.settings.mojom.Setting.kLiveCaption) {
      afterNextRender(this, () => {
        const captionsSubpage = /** @type {?SettingsCaptionsElement} */ (
            this.$$('settings-captions'));
        if (captionsSubpage && captionsSubpage.getLiveCaptionToggle()) {
          this.showDeepLinkElement(/** @type {!SettingsToggleButtonElement} */ (
              captionsSubpage.getLiveCaptionToggle()));
          return;
        }
        console.warn(`Element with deep link id ${settingId} not focusable.`);
      });

      // Stop deep link attempt since we completed it manually.
      return false;
    }

    // Continue with deep linking attempt.
    return true;
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    if (route === routes.OS_ACCESSIBILITY ||
        route === routes.MANAGE_CAPTION_SETTINGS) {
      this.attemptDeepLink();
    }
  },

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_(hasScreenReader) {
    this.showAccessibilityLabelsSetting_ = hasScreenReader;
  },

  /** @private */
  onToggleAccessibilityImageLabels_() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      this.browserProxy_.confirmA11yImageLabels();
    }
  },

  /** @private */
  onManageAccessibilityFeaturesTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  },

});

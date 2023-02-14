// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-and-magnification-page' is the accessibility settings
 * subpage for display and magnification accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './display_and_magnification_page.html.js';

const SettingsDisplayAndMagnificationElementBase = DeepLinkingMixin(
    RouteOriginMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

class SettingsDisplayAndMagnificationElement extends
    SettingsDisplayAndMagnificationElementBase {
  static get is() {
    return 'settings-display-and-magnification-page';
  }

  static get template() {
    return getTemplate();
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
       * Enum values for the
       * 'settings.a11y.screen_magnifier_mouse_following_mode' preference. These
       * values map to AccessibilityController::MagnifierMouseFollowingMode, and
       * are written to prefs and metrics, so order should not be changed.
       */
      screenMagnifierMouseFollowingModePrefValues_: {
        readOnly: true,
        type: Object,
        value: {
          CONTINUOUS: 0,
          CENTERED: 1,
          EDGE: 2,
        },
      },

      screenMagnifierZoomOptions_: {
        readOnly: true,
        type: Array,
        value() {
          // These values correspond to the i18n values in
          // settings_strings.grdp. If these values get changed then those
          // strings need to be changed as well.
          return [
            {value: 2, name: loadTimeData.getString('screenMagnifierZoom2x')},
            {value: 4, name: loadTimeData.getString('screenMagnifierZoom4x')},
            {value: 6, name: loadTimeData.getString('screenMagnifierZoom6x')},
            {value: 8, name: loadTimeData.getString('screenMagnifierZoom8x')},
            {value: 10, name: loadTimeData.getString('screenMagnifierZoom10x')},
            {value: 12, name: loadTimeData.getString('screenMagnifierZoom12x')},
            {value: 14, name: loadTimeData.getString('screenMagnifierZoom14x')},
            {value: 16, name: loadTimeData.getString('screenMagnifierZoom16x')},
            {value: 18, name: loadTimeData.getString('screenMagnifierZoom18x')},
            {value: 20, name: loadTimeData.getString('screenMagnifierZoom20x')},
          ];
        },
      },

      experimentalColorEnhancementSettingsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'areExperimentalAccessibilityColorEnhancementSettingsEnabled');
        },
      },

      /**
       * Whether the user is in kiosk mode.
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kFullscreenMagnifier,
          Setting.kFullscreenMagnifierMouseFollowingMode,
          Setting.kFullscreenMagnifierFocusFollowing,
          Setting.kDockedMagnifier,
        ]),
      },
    };
  }

  prefs: {[key: string]: any};
  private isKioskModeActive_: boolean;
  private experimentalColorEnhancementSettingsEnabled_: boolean;
  private route_: Route;
  private screenMagnifierMouseFollowingModePrefValues_: {[key: string]: number};
  private screenMagnifierZoomOptions_: Array<{value: number, name: string}>;


  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_DISPLAY_AND_MAGNIFICATION;
  }

  override ready() {
    super.ready();

    this.addFocusConfig(routes.DISPLAY, '#displaySubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_DISPLAY_AND_MAGNIFICATION) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Return Fullscreen magnifier description text based on whether Fullscreen
   * magnifier is enabled.
   */
  private getScreenMagnifierDescription_(enabled: boolean): string {
    return this.i18n(
        enabled ? 'screenMagnifierDescriptionOn' :
                  'screenMagnifierDescriptionOff');
  }

  private onDisplayTap_(): void {
    Router.getInstance().navigateTo(
        routes.DISPLAY,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  private onAppearanceTap_(): void {
    chrome.send('showBrowserAppearanceSettings');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-display-and-magnification-page':
        SettingsDisplayAndMagnificationElement;
  }
}

customElements.define(
    SettingsDisplayAndMagnificationElement.is,
    SettingsDisplayAndMagnificationElement);

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-and-magnification-page' is the accessibility settings
 * subpage for display and magnification accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl} from '../route_origin_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsDisplayAndMagnificationElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsDisplayAndMagnificationElement extends
    SettingsDisplayAndMagnificationElementBase {
  static get is() {
    return 'settings-display-and-magnification-page';
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
       * Enum values for the
       * 'settings.a11y.screen_magnifier_mouse_following_mode' preference. These
       * values map to AccessibilityController::MagnifierMouseFollowingMode, and
       * are written to prefs and metrics, so order should not be changed.
       * @protected {!Object<string, number>}
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

      /** @protected */
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

      /** @protected */
      isMagnifierContinuousMouseFollowingModeSettingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isMagnifierContinuousMouseFollowingModeSettingEnabled');
        },
      },

      /**
       * Whether the user is in kiosk mode.
       * @protected
       */
       isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /** @protected */
      screenMagnifierHintLabel_: {
        type: String,
        value() {
          return this.i18n(
              'screenMagnifierHintLabel',
              this.i18n('screenMagnifierHintSearchKey'));
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kFullscreenMagnifier,
          Setting.kFullscreenMagnifierMouseFollowingMode,
          Setting.kFullscreenMagnifierFocusFollowing,
          Setting.kDockedMagnifier,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_DISPLAY_AND_MAGNIFICATION;
  }

  /**
   * Note: Overrides RouteOriginBehavior implementation
   * @param {!Route} newRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {
    RouteOriginBehaviorImpl.currentRouteChanged.call(this, newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_DISPLAY_AND_MAGNIFICATION) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onDisplayTap_() {
    Router.getInstance().navigateTo(
        routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /** @private */
  onAppearanceTap_() {
    // Open browser appearance section in a new browser tab.
    window.open('chrome://settings/appearance');
  }
}

customElements.define(
    SettingsDisplayAndMagnificationElement.is,
    SettingsDisplayAndMagnificationElement);

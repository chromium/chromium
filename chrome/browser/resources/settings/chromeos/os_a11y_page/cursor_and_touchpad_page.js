// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cursor-and-touchpad-page' is the accessibility settings subpage
 * for cursor and touchpad accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
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
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {routes} from '../os_route.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {CursorAndTouchpadPageBrowserProxy, CursorAndTouchpadPageBrowserProxyImpl} from './cursor_and_touchpad_page_browser_proxy.js';

/** @const {number} */
const DEFAULT_BLACK_CURSOR_COLOR = 0;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCursorAndTouchpadPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsCursorAndTouchpadPageElement extends
    SettingsCursorAndTouchpadPageElementBase {
  static get is() {
    return 'settings-cursor-and-touchpad-page';
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
       * Drop down menu options for auto click delay.
       * @protected
       */
      autoClickDelayOptions_: {
        readOnly: true,
        type: Array,
        value() {
          // These values correspond to the i18n values in
          // settings_strings.grdp. If these values get changed then those
          // strings need to be changed as well.
          return [
            {
              value: 600,
              name: loadTimeData.getString('delayBeforeClickExtremelyShort'),
            },
            {
              value: 800,
              name: loadTimeData.getString('delayBeforeClickVeryShort'),
            },
            {
              value: 1000,
              name: loadTimeData.getString('delayBeforeClickShort'),
            },
            {value: 2000, name: loadTimeData.getString('delayBeforeClickLong')},
            {
              value: 4000,
              name: loadTimeData.getString('delayBeforeClickVeryLong'),
            },
          ];
        },
      },

      /**
       * Drop down menu options for auto click movement threshold.
       * @protected
       */
      autoClickMovementThresholdOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 5,
              name: loadTimeData.getString(
                  'autoclickMovementThresholdExtraSmall'),
            },
            {
              value: 10,
              name: loadTimeData.getString('autoclickMovementThresholdSmall'),
            },
            {
              value: 20,
              name: loadTimeData.getString('autoclickMovementThresholdDefault'),
            },
            {
              value: 30,
              name: loadTimeData.getString('autoclickMovementThresholdLarge'),
            },
            {
              value: 40,
              name: loadTimeData.getString(
                  'autoclickMovementThresholdExtraLarge'),
            },
          ];
        },
      },

      /** @protected {!Array<{name: string, value: number}>} */
      cursorColorOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: DEFAULT_BLACK_CURSOR_COLOR,
              name: loadTimeData.getString('cursorColorBlack'),
            },
            {
              value: 0xd93025,  // Red 600
              name: loadTimeData.getString('cursorColorRed'),
            },
            {
              value: 0xf29900,  //  Yellow 700
              name: loadTimeData.getString('cursorColorYellow'),
            },
            {
              value: 0x1e8e3e,  // Green 600
              name: loadTimeData.getString('cursorColorGreen'),
            },
            {
              value: 0x03b6be,  // Cyan 600
              name: loadTimeData.getString('cursorColorCyan'),
            },
            {
              value: 0x1a73e8,  // Blue 600
              name: loadTimeData.getString('cursorColorBlue'),
            },
            {
              value: 0xc61ad9,  // Magenta 600
              name: loadTimeData.getString('cursorColorMagenta'),
            },
            {
              value: 0xf50057,  // Pink A400
              name: loadTimeData.getString('cursorColorPink'),
            },

          ];
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

      /**
       * Whether a setting for enabling shelf navigation buttons in tablet mode
       * should be displayed in the accessibility settings.
       * @protected
       */
      showShelfNavigationButtonsSettings_: {
        type: Boolean,
        computed:
            'computeShowShelfNavigationButtonsSettings_(isKioskModeActive_)',
      },

      /**
       * Boolean indicating whether shelf navigation buttons should implicitly
       * be enabled in tablet mode - the navigation buttons are implicitly
       * enabled when spoken feedback, automatic clicks, or switch access are
       * enabled. The buttons can also be explicitly enabled by a designated
       * a11y setting.
       * @protected
       */
      shelfNavigationButtonsImplicitlyEnabled_: {
        type: Boolean,
        computed: 'computeShelfNavigationButtonsImplicitlyEnabled_(' +
            'prefs.settings.accessibility.value,' +
            'prefs.settings.a11y.autoclick.value,' +
            'prefs.settings.a11y.switch_access.enabled.value)',
      },

      /**
       * The effective pref value that indicates whether shelf navigation
       * buttons are enabled in tablet mode.
       * @type {chrome.settingsPrivate.PrefObject}
       * @protected
       */
      shelfNavigationButtonsPref_: {
        type: Object,
        computed: 'getShelfNavigationButtonsEnabledPref_(' +
            'shelfNavigationButtonsImplicitlyEnabled_,' +
            'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled)',
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kAutoClickWhenCursorStops,
          Setting.kLargeCursor,
          Setting.kHighlightCursorWhileMoving,
          Setting.kTabletNavigationButtons,
          Setting.kEnableCursorColor,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'pointersChanged(hasMouse_, hasPointingStick_, hasTouchpad_, ' +
          'isKioskModeActive_)',
    ];
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_CURSOR_AND_TOUCHPAD;

    /** @private {!CursorAndTouchpadPageBrowserProxy} */
    this.cursorAndTouchpadBrowserProxy_ =
        CursorAndTouchpadPageBrowserProxyImpl.getInstance();

    /** @private {!DevicePageBrowserProxy} */
    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'has-mouse-changed', (exists) => this.set('hasMouse_', exists));
    this.addWebUIListener(
        'has-pointing-stick-changed',
        (exists) => this.set('hasPointingStick_', exists));
    this.addWebUIListener(
        'has-touchpad-changed', (exists) => this.set('hasTouchpad_', exists));
    this.deviceBrowserProxy_.initializePointers();
  }

  /** @override */
  ready() {
    super.ready();

    this.addFocusConfig(routes.POINTERS, '#pointerSubpageButton');
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
    if (newRoute !== routes.A11Y_CURSOR_AND_TOUCHPAD) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @protected
   */
  pointersChanged(hasMouse, hasTouchpad, hasPointingStick, isKioskModeActive) {
    this.$.pointerSubpageButton.hidden =
        (!hasMouse && !hasPointingStick && !hasTouchpad) || isKioskModeActive;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowShelfNavigationButtonsSettings_() {
    return !this.isKioskModeActive_ &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  }

  /**
   * @return {boolean} Whether shelf navigation buttons should implicitly be
   *     enabled in tablet mode (due to accessibility settings different than
   *     shelf_navigation_buttons_enabled_in_tablet_mode).
   * @private
   */
  computeShelfNavigationButtonsImplicitlyEnabled_() {
    /**
     * Gets the bool pref value for the provided pref key.
     * @param {string} key
     * @return {boolean}
     */
    const getBoolPrefValue = (key) => {
      const pref = /** @type {chrome.settingsPrivate.PrefObject} */ (
          this.get(key, this.prefs));
      return pref && !!pref.value;
    };

    return getBoolPrefValue('settings.accessibility') ||
        getBoolPrefValue('settings.a11y.autoclick') ||
        getBoolPrefValue('settings.a11y.switch_access.enabled');
  }

  /**
   * Calculates the effective value for "shelf navigation buttons enabled in
   * tablet mode" setting - if the setting is implicitly enabled (by other a11y
   * settings), this will return a stub pref value.
   * @private
   * @return {chrome.settingsPrivate.PrefObject}
   */
  getShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return /** @type {!chrome.settingsPrivate.PrefObject}*/ ({
        value: true,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        key: '',
      });
    }

    return /** @type {chrome.settingsPrivate.PrefObject} */ (this.get(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled', this.prefs));
  }

  /** @private */
  onShelfNavigationButtonsLearnMoreClicked_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  }

  /**
   * Handles the <code>tablet_mode_shelf_nav_buttons_enabled</code> setting's
   * toggle changes. It updates the backing pref value, unless the setting is
   * implicitly enabled.
   * @private
   */
  updateShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return;
    }

    const enabled =
        this.shadowRoot.querySelector('#shelfNavigationButtonsEnabledControl')
            .checked;
    this.set(
        'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value',
        enabled);
    this.cursorAndTouchpadBrowserProxy_
        .recordSelectedShowShelfNavigationButtonValue(enabled);
  }

  /** @private */
  onA11yCursorColorChange_() {
    // Custom cursor color is enabled when the color is not set to black.
    const a11yCursorColorOn =
        this.get('prefs.settings.a11y.cursor_color.value') !==
        DEFAULT_BLACK_CURSOR_COLOR;
    this.set(
        'prefs.settings.a11y.cursor_color_enabled.value', a11yCursorColorOn);
  }


  /** @private */
  onMouseTap_() {
    Router.getInstance().navigateTo(
        routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  }
}

customElements.define(
    SettingsCursorAndTouchpadPageElement.is,
    SettingsCursorAndTouchpadPageElement);

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cursor-and-touchpad-page' is the accessibility settings subpage
 * for cursor and touchpad accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './cursor_and_touchpad_page.html.js';
import {CursorAndTouchpadPageBrowserProxy, CursorAndTouchpadPageBrowserProxyImpl} from './cursor_and_touchpad_page_browser_proxy.js';

const DEFAULT_BLACK_CURSOR_COLOR: number = 0;

interface Option {
  name: string;
  value: number;
}

interface SettingsCursorAndTouchpadPageElement {
  $: {
    pointerSubpageButton: CrLinkRowElement,
  };
}

const SettingsCursorAndTouchpadPageElementBase =
    DeepLinkingMixin(RouteOriginMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

class SettingsCursorAndTouchpadPageElement extends
    SettingsCursorAndTouchpadPageElementBase {
  static get is() {
    return 'settings-cursor-and-touchpad-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Drop down menu options for auto click delay.
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
       */
      shelfNavigationButtonsPref_: {
        type: Object,
        computed: 'getShelfNavigationButtonsEnabledPref_(' +
            'shelfNavigationButtonsImplicitlyEnabled_,' +
            'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled)',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
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

  private autoClickDelayOptions_: Option[];
  private autoClickMovementThresholdOptions_: Option[];
  private cursorAndTouchpadBrowserProxy_: CursorAndTouchpadPageBrowserProxy;
  private cursorColorOptions_: Option[];
  private deviceBrowserProxy_: DevicePageBrowserProxy;
  private isKioskModeActive_: boolean;
  private route_: Route;
  private shelfNavigationButtonsImplicitlyEnabled_: boolean;
  private shelfNavigationButtonsPref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  private showShelfNavigationButtonsSettings_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_CURSOR_AND_TOUCHPAD;

    this.cursorAndTouchpadBrowserProxy_ =
        CursorAndTouchpadPageBrowserProxyImpl.getInstance();

    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'has-mouse-changed',
        (exists: boolean) => this.set('hasMouse_', exists));
    this.addWebUiListener(
        'has-pointing-stick-changed',
        (exists: boolean) => this.set('hasPointingStick_', exists));
    this.addWebUiListener(
        'has-touchpad-changed',
        (exists: boolean) => this.set('hasTouchpad_', exists));
    this.deviceBrowserProxy_.initializePointers();
  }

  override ready() {
    super.ready();

    this.addFocusConfig(routes.POINTERS, '#pointerSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_CURSOR_AND_TOUCHPAD) {
      return;
    }

    this.attemptDeepLink();
  }

  pointersChanged(
      hasMouse: boolean, hasTouchpad: boolean, hasPointingStick: boolean,
      isKioskModeActive: boolean) {
    this.$.pointerSubpageButton.hidden =
        (!hasMouse && !hasPointingStick && !hasTouchpad) || isKioskModeActive;
  }

  private computeShowShelfNavigationButtonsSettings_(): boolean {
    return !this.isKioskModeActive_ &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  }

  /**
   * @return Whether shelf navigation buttons should implicitly be
   *     enabled in tablet mode (due to accessibility settings different than
   *     shelf_navigation_buttons_enabled_in_tablet_mode).
   */
  private computeShelfNavigationButtonsImplicitlyEnabled_(): boolean {
    /**
     * Gets the bool pref value for the provided pref key.
     */
    const getBoolPrefValue = (key: string): boolean => {
      const pref = this.getPref(key);
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
   */
  private getShelfNavigationButtonsEnabledPref_():
      chrome.settingsPrivate.PrefObject<boolean> {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return {
        value: true,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        key: '',
      };
    }

    return this.getPref<boolean>(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled');
  }

  private onShelfNavigationButtonsLearnMoreClicked_(): void {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  }

  /**
   * Handles the <code>tablet_mode_shelf_nav_buttons_enabled</code> setting's
   * toggle changes. It updates the backing pref value, unless the setting is
   * implicitly enabled.
   */
  private updateShelfNavigationButtonsEnabledPref_(): void {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return;
    }

    const enabled = this.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#shelfNavigationButtonsEnabledControl')!.checked;
    this.setPrefValue(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled', enabled);
    this.cursorAndTouchpadBrowserProxy_
        .recordSelectedShowShelfNavigationButtonValue(enabled);
  }

  private onA11yCursorColorChange_(): void {
    // Custom cursor color is enabled when the color is not set to black.
    const a11yCursorColorOn =
        this.getPref<number>('settings.a11y.cursor_color').value !==
        DEFAULT_BLACK_CURSOR_COLOR;
    this.set(
        'prefs.settings.a11y.cursor_color_enabled.value', a11yCursorColorOn);
  }


  private onMouseTap_(): void {
    Router.getInstance().navigateTo(
        routes.POINTERS,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cursor-and-touchpad-page': SettingsCursorAndTouchpadPageElement;
  }
}

customElements.define(
    SettingsCursorAndTouchpadPageElement.is,
    SettingsCursorAndTouchpadPageElement);

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard-and-text-input-page' is the accessibility settings subpage
 * for keyboard and text input accessibility settings.
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
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {KeyboardAndTextInputPageBrowserProxy, KeyboardAndTextInputPageBrowserProxyImpl} from './keyboard_and_text_input_page_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsKeyboardAndTextInputPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsKeyboardAndTextInputPageElement extends
    SettingsKeyboardAndTextInputPageElementBase {
  static get is() {
    return 'settings-keyboard-and-text-input-page';
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
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kStickyKeys,
          Setting.kOnScreenKeyboard,
          Setting.kDictation,
          Setting.kHighlightKeyboardFocus,
          Setting.kHighlightTextCaret,
          Setting.kEnableSwitchAccess,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_KEYBOARD_AND_TEXT_INPUT;

    /** @private {!KeyboardAndTextInputPageBrowserProxy} */
    this.keyboardAndTextInputBrowserProxy_ =
        KeyboardAndTextInputPageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.keyboardAndTextInputBrowserProxy_.keyboardAndTextInputPageReady();

    const r = routes;
    this.addFocusConfig(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig(r.KEYBOARD, '#keyboardSubpageButton');
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
    if (newRoute !== routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onSwitchAccessSettingsTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  /** @private */
  onKeyboardTap_() {
    Router.getInstance().navigateTo(
        routes.KEYBOARD,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onA11yCaretBrowsingChange_(event) {
    if (event.target.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  }
}

customElements.define(
    SettingsKeyboardAndTextInputPageElement.is,
    SettingsKeyboardAndTextInputPageElement);

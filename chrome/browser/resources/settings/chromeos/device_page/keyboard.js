// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import '../../controls/settings_dropdown_menu.js';

import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';

/**
 * Modifier key IDs corresponding to the ModifierKey enumerators in
 * /ui/base/ime/ash/ime_keyboard.h.
 * @enum {number}
 */
const ModifierKey = {
  SEARCH_KEY: 0,
  CONTROL_KEY: 1,
  ALT_KEY: 2,
  VOID_KEY: 3,  // Represents a disabled key.
  CAPS_LOCK_KEY: 4,
  ESCAPE_KEY: 5,
  BACKSPACE_KEY: 6,
  ASSISTANT_KEY: 7,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsKeyboardElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class SettingsKeyboardElement extends SettingsKeyboardElementBase {
  static get is() {
    return 'settings-keyboard';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private {!Map<string, (string|Function)>} */
      focusConfig: {
        type: Object,
        observer: 'onFocusConfigChange_',
      },

      /** @private Whether to show Caps Lock options. */
      showCapsLock_: Boolean,

      /**
       * @private
       * Whether this device has a ChromeOS launcher key. Applies only to
       * ChromeOS keyboards, internal or external.
       */
      hasLauncherKey_: Boolean,

      /** @private Whether this device has an Assistant key on keyboard. */
      hasAssistantKey_: Boolean,

      /**
       * Whether to show a remapping option for external keyboard's Meta key
       * (Search/Windows keys). This is true only when there's an external
       * keyboard connected that is a non-Apple keyboard.
       * @private
       */
      showExternalMetaKey_: Boolean,

      /**
       * Whether to show a remapping option for the Command key. This is true
       * when one of the connected keyboards is an Apple keyboard.
       * @private
       */
      showAppleCommandKey_: Boolean,

      /** @private {!DropdownMenuOptionList} Menu items for key mapping. */
      keyMapTargets_: Object,

      /**
       * Auto-repeat delays (in ms) for the corresponding slider values, from
       * long to short. The values were chosen to provide a large range while
       * giving several options near the defaults.
       * @private {!Array<number>}
       */
      autoRepeatDelays_: {
        type: Array,
        value: [2000, 1500, 1000, 500, 300, 200, 150],
        readOnly: true,
      },

      /**
       * Auto-repeat intervals (in ms) for the corresponding slider values, from
       * long to short. The slider itself is labeled "rate", the inverse of
       * interval, and goes from slow (long interval) to fast (short interval).
       * @private {!Array<number>}
       */
      autoRepeatIntervals_: {
        type: Array,
        value: [2000, 1000, 500, 300, 200, 100, 50, 30, 20],
        readOnly: true,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kKeyboardFunctionKeys,
          chromeos.settings.mojom.Setting.kKeyboardAutoRepeat,
          chromeos.settings.mojom.Setting.kKeyboardShortcuts,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!DevicePageBrowserProxy} */
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'show-keys-changed', this.onShowKeysChange_.bind(this));
    this.browserProxy_.initializeKeyboard();
    this.setUpKeyMapTargets_();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.KEYBOARD) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Initializes the dropdown menu options for remapping keys.
   * @private
   */
  setUpKeyMapTargets_() {
    // Ordering is according to UX, but values match ModifierKey.
    this.keyMapTargets_ = [
      {
        value: ModifierKey.SEARCH_KEY,
        name: loadTimeData.getString('keyboardKeySearch'),
      },
      {
        value: ModifierKey.CONTROL_KEY,
        name: loadTimeData.getString('keyboardKeyCtrl')
      },
      {
        value: ModifierKey.ALT_KEY,
        name: loadTimeData.getString('keyboardKeyAlt')
      },
      {
        value: ModifierKey.CAPS_LOCK_KEY,
        name: loadTimeData.getString('keyboardKeyCapsLock')
      },
      {
        value: ModifierKey.ESCAPE_KEY,
        name: loadTimeData.getString('keyboardKeyEscape')
      },
      {
        value: ModifierKey.BACKSPACE_KEY,
        name: loadTimeData.getString('keyboardKeyBackspace')
      },
      {
        value: ModifierKey.ASSISTANT_KEY,
        name: loadTimeData.getString('keyboardKeyAssistant')
      },
      {
        value: ModifierKey.VOID_KEY,
        name: loadTimeData.getString('keyboardKeyDisabled')
      }
    ];
  }

  /** @private */
  onFocusConfigChange_() {
    this.focusConfig.set(routes.OS_LANGUAGES_INPUT.path, () => {
      afterNextRender(this, () => {
        focusWithoutInk(
            assert(this.shadowRoot.querySelector('#showLanguagesInput')));
      });
    });
  }

  /**
   * Handler for updating which keys to show.
   * @param {Object} keyboardParams
   * @private
   */
  onShowKeysChange_(keyboardParams) {
    this.hasLauncherKey_ = keyboardParams['hasLauncherKey'];
    this.hasAssistantKey_ = keyboardParams['hasAssistantKey'];
    this.showCapsLock_ = keyboardParams['showCapsLock'];
    this.showExternalMetaKey_ = keyboardParams['showExternalMetaKey'];
    this.showAppleCommandKey_ = keyboardParams['showAppleCommandKey'];
  }

  /** @private */
  onShowKeyboardShortcutViewerTap_() {
    this.browserProxy_.showKeyboardShortcutViewer();
  }

  /** @private */
  onShowInputSettingsTap_() {
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_INPUT,
        /*dynamicParams=*/ null, /*removeSearch=*/ true);
  }

  /** @private */
  getExternalMetaKeyLabel_(hasLauncherKey) {
    return loadTimeData.getString(
        hasLauncherKey ? 'keyboardKeyExternalMeta' : 'keyboardKeyMeta');
  }

  /** @private */
  getExternalCommandKeyLabel_(hasLauncherKey) {
    return loadTimeData.getString(
        hasLauncherKey ? 'keyboardKeyExternalCommand' : 'keyboardKeyCommand');
  }
}

customElements.define(SettingsKeyboardElement.is, SettingsKeyboardElement);

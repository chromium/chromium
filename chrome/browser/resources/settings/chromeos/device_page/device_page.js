// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import './display.js';
import './keyboard.js';
import './pointers.js';
import './power.js';
import './storage.js';
import './storage_external.js';
import './stylus.js';
import '../../prefs/prefs.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage, getDisplayApi, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, PowerManagementSettings, PowerSource, StorageSpaceState} from './device_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-device-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    showCrostini: Boolean,

    /**
     * |hasMouse_|, |hasPointingStick_|, and |hasTouchpad_| start undefined so
     * observers don't trigger until they have been populated.
     * @private
     */
    hasMouse_: Boolean,

    /**
     * Whether a pointing stick (such as a TrackPoint) is connected.
     * @private
     */
    hasPointingStick_: Boolean,

    /** @private */
    hasTouchpad_: Boolean,

    /**
     * Whether the device has a haptic touchpad. If this is true, |hasTouchpad_|
     * will also be true.
     * @private
     */
    hasHapticTouchpad_: Boolean,

    /**
     * |hasStylus_| is initialized to false so that dom-if behaves correctly.
     * @private
     */
    hasStylus_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether storage management info should be hidden.
     * @private
     */
    hideStorageInfo_: {
      type: Boolean,
      value() {
        // TODO(crbug.com/868747): Show an explanatory message instead.
        return loadTimeData.valueExists('isDemoSession') &&
            loadTimeData.getBoolean('isDemoSession');
      },
      readOnly: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.POINTERS) {
          map.set(routes.POINTERS.path, '#pointersRow');
        }
        if (routes.KEYBOARD) {
          map.set(routes.KEYBOARD.path, '#keyboardRow');
        }
        if (routes.STYLUS) {
          map.set(routes.STYLUS.path, '#stylusRow');
        }
        if (routes.DISPLAY) {
          map.set(routes.DISPLAY.path, '#displayRow');
        }
        if (routes.STORAGE) {
          map.set(routes.STORAGE.path, '#storageRow');
        }
        if (routes.EXTERNAL_STORAGE_PREFERENCES) {
          map.set(
              routes.EXTERNAL_STORAGE_PREFERENCES.path,
              '#externalStoragePreferencesRow');
        }
        if (routes.POWER) {
          map.set(routes.POWER.path, '#powerRow');
        }
        return map;
      },
    },

    /** @private */
    androidEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('androidEnabled');
      },
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasPointingStick_, hasTouchpad_)',
  ],

  /** @override */
  attached() {
    this.addWebUIListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUIListener(
        'has-pointing-stick-changed', this.set.bind(this, 'hasPointingStick_'));
    this.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    this.addWebUIListener(
        'has-haptic-touchpad-changed',
        this.set.bind(this, 'hasHapticTouchpad_'));
    DevicePageBrowserProxyImpl.getInstance().initializePointers();

    this.addWebUIListener(
        'has-stylus-changed', this.set.bind(this, 'hasStylus_'));
    DevicePageBrowserProxyImpl.getInstance().initializeStylus();

    this.addWebUIListener(
        'storage-android-enabled-changed',
        this.set.bind(this, 'androidEnabled_'));
    DevicePageBrowserProxyImpl.getInstance().updateAndroidEnabled();
  },

  /**
   * @return {string}
   * @private
   */
  getPointersTitle_() {
    // For the purposes of the title, we call pointing sticks mice. The user
    // will know what we mean, and otherwise we'd get too many possible titles.
    const hasMouseOrPointingStick = this.hasMouse_ || this.hasPointingStick_;
    if (hasMouseOrPointingStick && this.hasTouchpad_) {
      return this.i18n('mouseAndTouchpadTitle');
    }
    if (hasMouseOrPointingStick) {
      return this.i18n('mouseTitle');
    }
    if (this.hasTouchpad_) {
      return this.i18n('touchpadTitle');
    }
    return '';
  },

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   * @private
   */
  onPointersTap_() {
    Router.getInstance().navigateTo(routes.POINTERS);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_() {
    Router.getInstance().navigateTo(routes.KEYBOARD);
  },

  /**
   * Handler for tapping the Stylus settings menu item.
   * @private
   */
  onStylusTap_() {
    Router.getInstance().navigateTo(routes.STYLUS);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_() {
    Router.getInstance().navigateTo(routes.DISPLAY);
  },

  /**
   * Handler for tapping the Storage settings menu item.
   * @private
   */
  onStorageTap_() {
    Router.getInstance().navigateTo(routes.STORAGE);
  },

  /**
   * Handler for tapping the Power settings menu item.
   * @private
   */
  onPowerTap_() {
    Router.getInstance().navigateTo(routes.POWER);
  },

  /** @protected */
  currentRouteChanged() {
    this.checkPointerSubpage_();
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_(hasMouse, hasPointingStick, hasTouchpad) {
    this.$.pointersRow.hidden = !hasMouse && !hasPointingStick && !hasTouchpad;
    this.checkPointerSubpage_();
  },

  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   * @private
   */
  checkPointerSubpage_() {
    // Check that the properties have explicitly been set to false.
    if (this.hasMouse_ === false && this.hasPointingStick_ === false &&
        this.hasTouchpad_ === false &&
        Router.getInstance().getCurrentRoute() === routes.POINTERS) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  },
});

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './audio.js';
import './display.js';
import './keyboard.js';
import './per_device_keyboard.js';
import './per_device_keyboard_remap_keys.js';
import './per_device_mouse.js';
import './per_device_pointing_stick.js';
import './per_device_touchpad.js';
import './pointers.js';
import './power.js';
import './storage.js';
import './storage_external.js';
import './stylus.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KeyboardPolicies, MousePolicies} from '../mojom-webui/input_device_settings.mojom-webui.js';
import {KeyboardSettingsObserverReceiver, MouseSettingsObserverReceiver, PointingStickSettingsObserverReceiver, TouchpadSettingsObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Router} from '../router.js';

import {getTemplate} from './device_page.html.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, Keyboard, Mouse, PointingStick, Touchpad} from './input_device_settings_types.js';
import {SettingsPerDeviceKeyboardRemapKeysElement} from './per_device_keyboard_remap_keys.js';

interface SettingsDevicePageElement {
  $: {
    pointersRow: CrLinkRowElement,
  };
}

const SettingsDevicePageElementBase =
    RouteObserverMixin(I18nMixin(WebUiListenerMixin(PolymerElement)));

class SettingsDevicePageElement extends SettingsDevicePageElementBase {
  static get is() {
    return 'settings-device-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      showCrostini: Boolean,

      /**
       * |hasMouse_|, |hasPointingStick_|, and |hasTouchpad_| start undefined so
       * observers don't trigger until they have been populated.
       */
      hasMouse_: Boolean,

      /**
       * Whether a pointing stick (such as a TrackPoint) is connected.
       */
      hasPointingStick_: Boolean,

      hasTouchpad_: Boolean,

      /**
       * Whether the device has a haptic touchpad. If this is true,
       * |hasTouchpad_| will also be true.
       */
      hasHapticTouchpad_: Boolean,

      /**
       * |hasStylus_| is initialized to false so that dom-if behaves correctly.
       */
      hasStylus_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether audio management info should be shown.
       */
      showAudioInfo_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableAudioSettingsPage');
        },
        readOnly: true,
      },

      /**
       * Whether settings should be split per device.
       */
      isDeviceSettingsSplitEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableInputDeviceSettingsSplit');
        },
        readOnly: true,
      },

      /**
       * Whether storage management info should be hidden.
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

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.POINTERS) {
            map.set(routes.POINTERS.path, '#pointersRow');
          }
          if (routes.PER_DEVICE_MOUSE) {
            map.set(routes.PER_DEVICE_MOUSE.path, '#perDeviceMouseRow');
          }
          if (routes.PER_DEVICE_TOUCHPAD) {
            map.set(routes.PER_DEVICE_TOUCHPAD.path, '#perDeviceTouchpadRow');
          }
          if (routes.PER_DEVICE_POINTING_STICK) {
            map.set(
                routes.PER_DEVICE_POINTING_STICK.path,
                '#perDevicePointingStickRow');
          }
          if (routes.PER_DEVICE_KEYBOARD) {
            map.set(routes.PER_DEVICE_KEYBOARD.path, '#perDeviceKeyboardRow');
          }
          if (routes.PER_DEVICE_KEYBOARD_REMAP_KEYS) {
            map.set(
                routes.PER_DEVICE_KEYBOARD_REMAP_KEYS.path,
                '#perDeviceKeyboardRemapKeysRow');
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
          if (routes.AUDIO) {
            map.set(routes.AUDIO.path, '#audioRow');
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

      androidEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('androidEnabled');
        },
      },

      pointingSticks: {
        type: Array,
      },

      keyboards: {
        type: Array,
      },

      keyboardPolicies: {
        type: Object,
      },

      touchpads: {
        type: Array,
      },

      mice: {
        type: Array,
      },

      mousePolicies: {
        type: Object,
      },
    };
  }

  static get observers() {
    return [
      'pointersChanged_(hasMouse_, hasPointingStick_, hasTouchpad_)',
      'mouseChanged_(hasMouse_)',
      'touchpadChanged_(hasTouchpad_)',
      'pointingStickChanged_(hasPointingStick_)',
    ];
  }

  protected pointingSticks: PointingStick[];
  protected keyboards: Keyboard[];
  protected keyboardPolicies: KeyboardPolicies;
  protected touchpads: Touchpad[];
  protected mice: Mouse[];
  protected mousePolicies: MousePolicies;
  private browserProxy_: DevicePageBrowserProxy;
  private hasMouse_: boolean;
  private hasPointingStick_: boolean;
  private hasTouchpad_: boolean;
  private isDeviceSettingsSplitEnabled_: boolean;
  private pointingStickSettingsObserverReceiver:
      PointingStickSettingsObserverReceiver;
  private keyboardSettingsObserverReceiver: KeyboardSettingsObserverReceiver;
  private touchpadSettingsObserverReceiver: TouchpadSettingsObserverReceiver;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface;
  private mouseSettingsObserverReceiver: MouseSettingsObserverReceiver;

  constructor() {
    super();

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
    if (this.isDeviceSettingsSplitEnabled_) {
      this.inputDeviceSettingsProvider = getInputDeviceSettingsProvider();
      this.observePointingStickSettings();
      this.observeKeyboardSettings();
      this.observeTouchpadSettings();
      this.observeMouseSettings();
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUiListener(
        'has-pointing-stick-changed', this.set.bind(this, 'hasPointingStick_'));
    this.addWebUiListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    this.addWebUiListener(
        'has-haptic-touchpad-changed',
        this.set.bind(this, 'hasHapticTouchpad_'));
    this.browserProxy_.initializePointers();

    this.addWebUiListener(
        'has-stylus-changed', this.set.bind(this, 'hasStylus_'));
    this.browserProxy_.initializeStylus();

    this.addWebUiListener(
        'storage-android-enabled-changed',
        this.set.bind(this, 'androidEnabled_'));
    this.browserProxy_.updateAndroidEnabled();
  }

  private observePointingStickSettings(): void {
    if (this.inputDeviceSettingsProvider instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider.observePointingStickSettings(this);
      return;
    }

    this.pointingStickSettingsObserverReceiver =
        new PointingStickSettingsObserverReceiver(this);

    this.inputDeviceSettingsProvider.observePointingStickSettings(
        this.pointingStickSettingsObserverReceiver.$
            .bindNewPipeAndPassRemote());
  }

  onPointingStickListUpdated(pointingSticks: PointingStick[]): void {
    this.pointingSticks = pointingSticks;
  }

  private observeKeyboardSettings(): void {
    if (this.inputDeviceSettingsProvider instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider.observeKeyboardSettings(this);
      return;
    }

    this.keyboardSettingsObserverReceiver =
        new KeyboardSettingsObserverReceiver(this);

    this.inputDeviceSettingsProvider.observeKeyboardSettings(
        this.keyboardSettingsObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  onKeyboardListUpdated(keyboards: Keyboard[]): void {
    this.keyboards = keyboards;
  }

  onKeyboardPoliciesUpdated(keyboardPolicies: KeyboardPolicies): void {
    this.keyboardPolicies = keyboardPolicies;
  }

  private observeTouchpadSettings(): void {
    if (this.inputDeviceSettingsProvider instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider.observeTouchpadSettings(this);
      return;
    }

    this.touchpadSettingsObserverReceiver =
        new TouchpadSettingsObserverReceiver(this);

    this.inputDeviceSettingsProvider.observeTouchpadSettings(
        this.touchpadSettingsObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  onTouchpadListUpdated(touchpads: Touchpad[]): void {
    this.touchpads = touchpads;
  }

  private observeMouseSettings(): void {
    if (this.inputDeviceSettingsProvider instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider.observeMouseSettings(this);
      return;
    }

    this.mouseSettingsObserverReceiver =
        new MouseSettingsObserverReceiver(this);

    this.inputDeviceSettingsProvider.observeMouseSettings(
        this.mouseSettingsObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  onMouseListUpdated(mice: Mouse[]): void {
    this.mice = mice;
  }

  onMousePoliciesUpdated(mousePolicies: MousePolicies): void {
    this.mousePolicies = mousePolicies;
  }

  private getPointersTitle_(): string {
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
  }

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   */
  private onPointersClick_() {
    Router.getInstance().navigateTo(routes.POINTERS);
  }

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   */
  private onPerDeviceKeyboardClick_() {
    Router.getInstance().navigateTo(routes.PER_DEVICE_KEYBOARD);
  }

  /**
   * Handler for tapping the Mouse settings menu item.
   */
  private onPerDeviceMouseClick_() {
    Router.getInstance().navigateTo(routes.PER_DEVICE_MOUSE);
  }

  /**
   * Handler for tapping the Touchpad settings menu item.
   */
  private onPerDeviceTouchpadClick_() {
    Router.getInstance().navigateTo(routes.PER_DEVICE_TOUCHPAD);
  }

  /**
   * Handler for tapping the Pointing stick settings menu item.
   */
  private onPerDevicePointingStickClick_() {
    Router.getInstance().navigateTo(routes.PER_DEVICE_POINTING_STICK);
  }

  /**
   * Handler for tapping the Keyboard settings menu item.
   */
  private onKeyboardClick_() {
    Router.getInstance().navigateTo(routes.KEYBOARD);
  }

  /**
   * Handler for tapping the Stylus settings menu item.
   */
  private onStylusClick_() {
    Router.getInstance().navigateTo(routes.STYLUS);
  }

  /**
   * Handler for tapping the Display settings menu item.
   */
  private onDisplayClick_() {
    Router.getInstance().navigateTo(routes.DISPLAY);
  }

  /**
   * Handler for tapping the Audio settings menu item.
   */
  private onAudioClick_() {
    Router.getInstance().navigateTo(routes.AUDIO);
  }

  /**
   * Handler for tapping the Storage settings menu item.
   */
  private onStorageClick_() {
    Router.getInstance().navigateTo(routes.STORAGE);
  }

  /**
   * Handler for tapping the Power settings menu item.
   */
  private onPowerClick_() {
    Router.getInstance().navigateTo(routes.POWER);
  }

  override currentRouteChanged() {
    this.checkPointerSubpage_();
  }

  private pointersChanged_() {
    this.checkPointerSubpage_();
  }

  private mouseChanged_(hasMouse: boolean) {
    if (hasMouse === false &&
        Router.getInstance().currentRoute === routes.PER_DEVICE_MOUSE) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private touchpadChanged_(hasTouchpad: boolean) {
    if (hasTouchpad === false &&
        Router.getInstance().currentRoute === routes.PER_DEVICE_TOUCHPAD) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private pointingStickChanged_(hasPointingStick: boolean) {
    if (hasPointingStick === false &&
        Router.getInstance().currentRoute ===
            routes.PER_DEVICE_POINTING_STICK) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private showPointersRow_(): boolean {
    return (this.hasMouse_ || this.hasTouchpad_ || this.hasPointingStick_) &&
        !this.isDeviceSettingsSplitEnabled_;
  }

  private showPerDeviceMouseRow_(): boolean {
    return this.hasMouse_ && this.isDeviceSettingsSplitEnabled_;
  }

  private showPerDeviceTouchpadRow_(): boolean {
    return this.hasTouchpad_ && this.isDeviceSettingsSplitEnabled_;
  }

  private showPerDevicePointingStickRow_(): boolean {
    return this.hasPointingStick_ && this.isDeviceSettingsSplitEnabled_;
  }

  protected restoreDefaults(): void {
    const remapKeysPage = this.shadowRoot!.querySelector('#remap-keys') as
        SettingsPerDeviceKeyboardRemapKeysElement;
    remapKeysPage.restoreDefaults();
  }
  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   */
  private checkPointerSubpage_() {
    // Check that the properties have explicitly been set to false.
    if (this.hasMouse_ === false && this.hasPointingStick_ === false &&
        this.hasTouchpad_ === false &&
        Router.getInstance().currentRoute === routes.POINTERS) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-device-page': SettingsDevicePageElement;
  }
}

customElements.define(SettingsDevicePageElement.is, SettingsDevicePageElement);

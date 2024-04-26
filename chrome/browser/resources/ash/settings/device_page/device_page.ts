// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './audio.js';
import './display.js';
import './graphics_tablet_subpage.js';
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
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../os_printing_page/printing_settings_card.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isExternalStorageEnabled, isInputDeviceSettingsSplitEnabled, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {PrefsState} from '../common/types.js';
import {KeyboardPolicies, MousePolicies} from '../mojom-webui/input_device_settings.mojom-webui.js';
import {GraphicsTabletSettingsObserverReceiver, KeyboardSettingsObserverReceiver, MouseSettingsObserverReceiver, PointingStickSettingsObserverReceiver, TouchpadSettingsObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {ACCESSIBILITY_COMMON_IME_ID} from '../os_languages_page/languages.js';
import {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './device_page.html.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {GraphicsTablet, InputDeviceSettingsProviderInterface, Keyboard, Mouse, PointingStick, Touchpad} from './input_device_settings_types.js';
import {SettingsPerDeviceKeyboardRemapKeysElement} from './per_device_keyboard_remap_keys.js';

const SettingsDevicePageElementBase =
    RouteOriginMixin(I18nMixin(WebUiListenerMixin(PolymerElement)));

export class SettingsDevicePageElement extends SettingsDevicePageElementBase {
  static get is() {
    return 'settings-device-page' as const;
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

      section_: {
        type: Number,
        value: Section.kDevice,
        readOnly: true,
      },

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
       * Whether settings should be split per device.
       */
      isDeviceSettingsSplitEnabled_: {
        type: Boolean,
        value() {
          return isInputDeviceSettingsSplitEnabled();
        },
        readOnly: true,
      },

      /**
       * Whether users are allowed to customize buttons on their peripherals.
       */
      isPeripheralCustomizationEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePeripheralCustomization');
        },
        readOnly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      /**
       * Whether storage management info should be hidden.
       */
      hideStorageInfo_: {
        type: Boolean,
        value() {
          // TODO(crbug.com/40587075): Show an explanatory message instead.
          return loadTimeData.valueExists('isDemoSession') &&
              loadTimeData.getBoolean('isDemoSession');
        },
        readOnly: true,
      },

      isExternalStorageEnabled_: {
        type: Boolean,
        value() {
          return isExternalStorageEnabled();
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

      graphicsTablets: {
        type: Array,
      },

      /**
       * Set of languages from <settings-languages>
       */
      languages: Object,

      /**
       * Language helper API from <settings-languages>
       */
      languageHelper: Object,

      inputMethodDisplayName_: {
        type: String,
        computed: 'computeInputMethodDisplayName_(' +
            'languages.inputMethods.currentId, languageHelper)',
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              mouse: 'os-settings:device-mouse',
              touchpad: 'os-settings:device-touchpad',
              pointingStick: 'os-settings:device-pointing-stick',
              keyboardAndInputs: 'os-settings:device-keyboard',
              stylus: 'os-settings:device-stylus',
              tablet: 'os-settings:device-tablet',
              display: 'os-settings:device-display',
              audio: 'os-settings:device-audio',
            };
          }

          return {
            mouse: '',
            touchpad: '',
            pointingStick: '',
            keyboardAndInputs: '',
            stylus: '',
            tablet: '',
            display: '',
            audio: '',
          };
        },
      },
    };
  }

  static get observers() {
    return [
      'pointersChanged_(hasMouse_, hasPointingStick_, hasTouchpad_)',
      'mouseChanged_(mice)',
      'touchpadChanged_(touchpads)',
      'pointingStickChanged_(pointingSticks)',
      'graphicsTabletChanged_(graphicsTablets)',
    ];
  }

  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper|undefined;
  prefs: PrefsState|undefined;

  protected pointingSticks: PointingStick[];
  protected keyboards: Keyboard[];
  protected keyboardPolicies: KeyboardPolicies;
  protected touchpads: Touchpad[];
  protected mice: Mouse[];
  protected mousePolicies: MousePolicies;
  protected graphicsTablets: GraphicsTablet[];
  private browserProxy_: DevicePageBrowserProxy;
  private hasMouse_: boolean;
  private hasPointingStick_: boolean;
  private hasTouchpad_: boolean;
  private hasHapticTouchpad_: boolean;
  private isDeviceSettingsSplitEnabled_: boolean;
  private isPeripheralCustomizationEnabled: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private pointingStickSettingsObserverReceiver:
      PointingStickSettingsObserverReceiver;
  private keyboardSettingsObserverReceiver: KeyboardSettingsObserverReceiver;
  private touchpadSettingsObserverReceiver: TouchpadSettingsObserverReceiver;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface;
  private mouseSettingsObserverReceiver: MouseSettingsObserverReceiver;
  private graphicsTabletSettingsObserverReceiver:
      GraphicsTabletSettingsObserverReceiver;
  private rowIcons_: Record<string, string>;
  private section_: Section;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.DEVICE;

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
    if (this.isDeviceSettingsSplitEnabled_) {
      this.inputDeviceSettingsProvider = getInputDeviceSettingsProvider();
      this.observePointingStickSettings();
      this.observeKeyboardSettings();
      this.observeTouchpadSettings();
      this.observeMouseSettings();
      if (this.isPeripheralCustomizationEnabled) {
        // The flag `isPeripheralCustomizationEnabled` should only be enabled
        // when `isDeviceSettingsSplitEnabled_` is enabled. Will not call
        // `getInputDeviceSettingsProvider` here again.
        this.observeGraphicsTabletSettings();
      }
    }

  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (!this.isDeviceSettingsSplitEnabled_) {
      this.addWebUiListener(
          'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
      this.addWebUiListener(
          'has-pointing-stick-changed',
          this.set.bind(this, 'hasPointingStick_'));
      this.addWebUiListener(
          'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
      this.addWebUiListener(
          'has-haptic-touchpad-changed',
          this.set.bind(this, 'hasHapticTouchpad_'));
      this.browserProxy_.initializePointers();
    }

    this.addWebUiListener(
        'has-stylus-changed', this.set.bind(this, 'hasStylus_'));
    this.browserProxy_.initializeStylus();

    this.addWebUiListener(
        'storage-android-enabled-changed',
        this.set.bind(this, 'isExternalStorageEnabled_'));
    this.browserProxy_.updateAndroidEnabled();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.POINTERS, '#pointersRow');
    this.addFocusConfig(routes.PER_DEVICE_MOUSE, '#perDeviceMouseRow');
    this.addFocusConfig(routes.PER_DEVICE_TOUCHPAD, '#perDeviceTouchpadRow');
    this.addFocusConfig(
        routes.PER_DEVICE_POINTING_STICK, '#perDevicePointingStickRow');
    this.addFocusConfig(routes.PER_DEVICE_KEYBOARD, '#perDeviceKeyboardRow');
    this.addFocusConfig(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        '#perDeviceKeyboardRemapKeysRow');
    this.addFocusConfig(routes.KEYBOARD, '#keyboardRow');
    this.addFocusConfig(routes.STYLUS, '#stylusRow');
    this.addFocusConfig(routes.DISPLAY, '#displayRow');
    this.addFocusConfig(routes.AUDIO, '#audioRow');
    this.addFocusConfig(routes.GRAPHICS_TABLET, '#tabletRow');
    this.addFocusConfig(
        routes.CUSTOMIZE_MOUSE_BUTTONS, '#customizeMouseButtonsRow');
    this.addFocusConfig(
        routes.CUSTOMIZE_TABLET_BUTTONS, '#customizeTabletButtonsSubpage');
    this.addFocusConfig(
        routes.CUSTOMIZE_PEN_BUTTONS, '#customizePenButtonsSubpage');

    if (!this.isRevampWayfindingEnabled_) {
      this.addFocusConfig(routes.STORAGE, '#storageRow');
      this.addFocusConfig(routes.POWER, '#powerRow');
    }
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

  private observeGraphicsTabletSettings(): void {
    if (this.inputDeviceSettingsProvider instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider.observeGraphicsTabletSettings(this);
      return;
    }

    this.graphicsTabletSettingsObserverReceiver =
        new GraphicsTabletSettingsObserverReceiver(this);

    this.inputDeviceSettingsProvider.observeGraphicsTabletSettings(
        this.graphicsTabletSettingsObserverReceiver.$
            .bindNewPipeAndPassRemote());
  }

  onGraphicsTabletListUpdated(graphicsTablets: GraphicsTablet[]): void {
    this.graphicsTablets = graphicsTablets;
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
  private onPointersClick_(): void {
    Router.getInstance().navigateTo(routes.POINTERS);
  }

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   */
  private onPerDeviceKeyboardClick_(): void {
    Router.getInstance().navigateTo(routes.PER_DEVICE_KEYBOARD);
  }

  /**
   * Handler for tapping the Mouse settings menu item.
   */
  private onPerDeviceMouseClick_(): void {
    Router.getInstance().navigateTo(routes.PER_DEVICE_MOUSE);
  }

  /**
   * Handler for tapping the Touchpad settings menu item.
   */
  private onPerDeviceTouchpadClick_(): void {
    Router.getInstance().navigateTo(routes.PER_DEVICE_TOUCHPAD);
  }

  /**
   * Handler for tapping the Pointing stick settings menu item.
   */
  private onPerDevicePointingStickClick_(): void {
    Router.getInstance().navigateTo(routes.PER_DEVICE_POINTING_STICK);
  }

  /**
   * Handler for tapping the Keyboard settings menu item.
   */
  private onKeyboardClick_(): void {
    Router.getInstance().navigateTo(routes.KEYBOARD);
  }

  /**
   * Handler for tapping the Stylus settings menu item.
   */
  private onStylusClick_(): void {
    Router.getInstance().navigateTo(routes.STYLUS);
  }

  /**
   * Handler for tapping the Graphics tablet settings menu item.
   */
  private onGraphicsTabletClick(): void {
    Router.getInstance().navigateTo(routes.GRAPHICS_TABLET);
  }

  /**
   * Handler for tapping the Display settings menu item.
   */
  private onDisplayClick_(): void {
    Router.getInstance().navigateTo(routes.DISPLAY);
  }

  /**
   * Handler for tapping the Audio settings menu item.
   */
  private onAudioClick_(): void {
    Router.getInstance().navigateTo(routes.AUDIO);
  }

  /**
   * Handler for tapping the Storage settings menu item.
   */
  private onStorageClick_(): void {
    Router.getInstance().navigateTo(routes.STORAGE);
  }

  /**
   * Handler for tapping the Power settings menu item.
   */
  private onPowerClick_(): void {
    Router.getInstance().navigateTo(routes.POWER);
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    this.checkPointerSubpage_();
  }

  private pointersChanged_(): void {
    this.checkPointerSubpage_();
  }

  private mouseChanged_(): void {
    if ((!this.mice || this.mice.length === 0) &&
        Router.getInstance().currentRoute === routes.PER_DEVICE_MOUSE) {
      getAnnouncerInstance().announce(
          this.i18n('allMiceDisconnectedA11yLabel'));
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private touchpadChanged_(): void {
    if ((!this.touchpads || this.touchpads.length === 0) &&
        Router.getInstance().currentRoute === routes.PER_DEVICE_TOUCHPAD) {
      getAnnouncerInstance().announce(
          this.i18n('allTouchpadsDisconnectedA11yLabel'));

      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private pointingStickChanged_(): void {
    if ((!this.pointingSticks || this.pointingSticks.length === 0) &&
        Router.getInstance().currentRoute ===
            routes.PER_DEVICE_POINTING_STICK) {
      getAnnouncerInstance().announce(
          this.i18n('allPointingSticksDisconnectedA11yLabel'));
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private graphicsTabletChanged_(): void {
    if ((!this.graphicsTablets || this.graphicsTablets.length === 0) &&
        Router.getInstance().currentRoute === routes.GRAPHICS_TABLET) {
      getAnnouncerInstance().announce(
          this.i18n('allGraphicsTabletsDisconnectedA11yLabel'));
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  private showPointersRow_(): boolean {
    return (this.hasMouse_ || this.hasTouchpad_ || this.hasPointingStick_) &&
        !this.isDeviceSettingsSplitEnabled_;
  }

  private showPerDeviceMouseRow_(): boolean {
    return this.isDeviceSettingsSplitEnabled_ && this.mice &&
        this.mice.length !== 0;
  }

  private showPerDeviceTouchpadRow_(touchpads: Touchpad[]): boolean {
    return this.isDeviceSettingsSplitEnabled_ && touchpads &&
        touchpads.length !== 0;
  }

  private showPerDevicePointingStickRow_(): boolean {
    return this.isDeviceSettingsSplitEnabled_ && this.pointingSticks &&
        this.pointingSticks.length !== 0;
  }

  private showGraphicsTabletRow_(): boolean {
    return this.isPeripheralCustomizationEnabled && this.graphicsTablets &&
        this.graphicsTablets.length !== 0;
  }

  protected restoreDefaults(): void {
    const remapKeysPage =
        this.shadowRoot!
            .querySelector<SettingsPerDeviceKeyboardRemapKeysElement>(
                '#remap-keys')!;
    remapKeysPage.restoreDefaults();
  }
  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   */
  private checkPointerSubpage_(): void {
    // Check that the properties have explicitly been set to false.
    if (this.hasMouse_ === false && this.hasPointingStick_ === false &&
        this.hasTouchpad_ === false &&
        Router.getInstance().currentRoute === routes.POINTERS) {
      Router.getInstance().navigateTo(routes.DEVICE);
    }
  }

  /**
   * Computes the display name for the currently configured input method. This
   * should be displayed as a sublabel under the Keyboard and inputs row, only
   * when OsSettingsRevampWayfinding is enabled.
   */
  private computeInputMethodDisplayName_(): string {
    if (!this.isRevampWayfindingEnabled_) {
      return '';
    }

    const id = this.languages?.inputMethods?.currentId;
    if (!id || !this.languageHelper) {
      return '';
    }
    if (id === ACCESSIBILITY_COMMON_IME_ID) {
      return '';
    }
    return this.languageHelper.getInputMethodDisplayName(id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDevicePageElement.is]: SettingsDevicePageElement;
  }
}

customElements.define(SettingsDevicePageElement.is, SettingsDevicePageElement);

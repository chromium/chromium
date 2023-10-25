// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../settings_shared.css.js';
import '../os_settings_icons.html.js';
import './menu_item.js';

import {getDeviceName} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {BluetoothSystemProperties, DeviceConnectionState, PairedBluetoothDeviceProperties, SystemPropertiesObserverReceiver as BluetoothPropertiesObserverReceiver} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists, castExists} from '../assert_extras.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {FakeInputDeviceSettingsProvider} from '../device_page/fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from '../device_page/input_device_mojo_interface_provider.js';
import {InputDeviceSettingsProviderInterface, Keyboard, Mouse, PointingStick, Touchpad} from '../device_page/input_device_settings_types.js';
import {KeyboardSettingsObserverReceiver, MouseSettingsObserverReceiver, PointingStickSettingsObserverReceiver, TouchpadSettingsObserverReceiver} from '../mojom-webui/input_device_settings_provider.mojom-webui.js';
import * as routesMojom from '../mojom-webui/routes.mojom-webui.js';
import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from '../multidevice_page/multidevice_browser_proxy.js';
import {MultiDevicePageContentData, MultiDeviceSettingsMode} from '../multidevice_page/multidevice_constants.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {AccountManagerBrowserProxyImpl} from '../os_people_page/account_manager_browser_proxy.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {isAdvancedRoute, Route, Router} from '../router.js';

import {getTemplate} from './os_settings_menu.html.js';

const {Section} = routesMojom;

interface MenuItemData {
  section: routesMojom.Section;
  path: string;
  icon: string;
  label: string;

  // Sublabels should only exist when OsSettingsRevampWayfinding is enabled.
  sublabel?: string;
}

export interface OsSettingsMenuElement {
  $: {
    topMenu: IronSelectorElement,
    topMenuRepeat: DomRepeat,
  };
}

/**
 * Returns a copy of the given `str` with the first letter capitalized according
 * to the locale.
 */
function capitalize(str: string): string {
  const firstChar = str.charAt(0).toLocaleUpperCase();
  const remainingStr = str.slice(1);
  return `${firstChar}${remainingStr}`;
}

const OsSettingsMenuElementBase =
    WebUiListenerMixin(RouteObserverMixin(I18nMixin(PolymerElement)));

export class OsSettingsMenuElement extends OsSettingsMenuElementBase {
  static get is() {
    return 'os-settings-menu' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Determines which menu items are available for their respective pages
       */
      pageAvailability: {
        type: Object,
      },

      advancedOpened: {
        type: Boolean,
        value: false,
        notify: true,
      },

      basicMenuItems_: {
        type: Array,
        computed: 'computeBasicMenuItems_(pageAvailability.*,' +
            'accountsMenuItemDescription_,' +
            'bluetoothMenuItemDescription_,' +
            'deviceMenuItemDescription_,' +
            'multideviceMenuItemDescription_)',
        readOnly: true,
      },

      advancedMenuItems_: {
        type: Array,
        computed: 'computeAdvancedMenuItems_(pageAvailability.*)',
        readOnly: true,
      },

      /**
       * The path of the currently selected menu item. e.g. '/internet'.
       */
      selectedItemPath_: {
        type: String,
        value: '',
      },

      aboutMenuItemPath_: {
        type: String,
        value: `/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },

      accountsMenuItemDescription_: {
        type: String,
        value(this: OsSettingsMenuElement) {
          return this.i18n('primaryUserEmail');
        },
      },

      bluetoothMenuItemDescription_: {
        type: String,
        value: '',
      },

      hasKeyboard_: Boolean,

      hasMouse_: Boolean,

      /**
       * Whether a pointing stick (such as a TrackPoint) is connected.
       */
      hasPointingStick_: Boolean,

      hasTouchpad_: Boolean,

      deviceMenuItemDescription_: {
        type: String,
        value: '',
        computed: 'computeDeviceMenuItemDescription_(hasKeyboard_,' +
            'hasMouse_, hasPointingStick_, hasTouchpad_, hasHapticTouchpad_)',
      },

      multideviceMenuItemDescription_: {
        type: String,
        value: '',
      },
    };
  }

  advancedOpened: boolean;
  pageAvailability: OsPageAvailability;
  private basicMenuItems_: MenuItemData[];
  private advancedMenuItems_: MenuItemData[];
  private isRevampWayfindingEnabled_: boolean;
  private selectedItemPath_: string;
  private aboutMenuItemPath_: string;

  // Accounts section members.
  private accountsMenuItemDescription_: string;

  // Bluetooth section members.
  private bluetoothMenuItemDescription_: string;
  private bluetoothPropertiesObserverReceiver_:
      BluetoothPropertiesObserverReceiver|undefined;

  // Device section members.
  private deviceMenuItemDescription_: string;
  private hasKeyboard_: boolean|undefined;
  private hasMouse_: boolean|undefined;
  private hasPointingStick_: boolean|undefined;
  private hasTouchpad_: boolean|undefined;
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface;
  private keyboardSettingsObserverReceiver_: KeyboardSettingsObserverReceiver|
      undefined;
  private mouseSettingsObserverReceiver_: MouseSettingsObserverReceiver|
      undefined;
  private pointingStickSettingsObserverReceiver_:
      PointingStickSettingsObserverReceiver|undefined;
  private touchpadSettingsObserverReceiver_: TouchpadSettingsObserverReceiver|
      undefined;

  // Multidevice section members.
  private multideviceBrowserProxy_: MultiDeviceBrowserProxy;
  private multideviceMenuItemDescription_: string;

  constructor() {
    super();

    this.inputDeviceSettingsProvider_ = getInputDeviceSettingsProvider();
    this.multideviceBrowserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (this.isRevampWayfindingEnabled_) {
      // Accounts menu item.
      this.updateAccountsMenuItemDescription_();
      this.addWebUiListener(
          'accounts-changed',
          this.updateAccountsMenuItemDescription_.bind(this));

      // Bluetooth menu item.
      this.observeBluetoothProperties_();

      // Device menu item.
      this.observeKeyboardSettings_();
      this.observeMouseSettings_();
      this.observePointingStickSettings_();
      this.observeTouchpadSettings_();

      // Multidevice menu item.
      this.addWebUiListener(
          'settings.updateMultidevicePageContentData',
          this.updateMultideviceMenuItemDescription_.bind(this));
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.bluetoothPropertiesObserverReceiver_?.$.close();

    // The following receivers are undefined in tests.
    this.keyboardSettingsObserverReceiver_?.$.close();
    this.mouseSettingsObserverReceiver_?.$.close();
    this.pointingStickSettingsObserverReceiver_?.$.close();
    this.touchpadSettingsObserverReceiver_?.$.close();
  }

  override ready(): void {
    super.ready();

    // Force render menu items so the matching item can be selected when the
    // page initially loads.
    this.$.topMenuRepeat.render();

    if (this.isRevampWayfindingEnabled_) {
      this.multideviceBrowserProxy_.getPageContentData().then(
          this.updateMultideviceMenuItemDescription_.bind(this));
    }
  }

  override currentRouteChanged(newRoute: Route): void {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && isAdvancedRoute(newRoute)) {
      this.advancedOpened = true;
    }

    this.setSelectedItemPathForRoute_(newRoute);
  }

  /**
   * Set the selected menu item based on a menu item's route matching or
   * containing the given |route|.
   */
  private setSelectedItemPathForRoute_(route: Route): void {
    const menuItems =
        this.shadowRoot!.querySelectorAll('os-settings-menu-item');
    for (const menuItem of menuItems) {
      const matchingRoute = Router.getInstance().getRouteForPath(menuItem.path);
      if (matchingRoute?.contains(route)) {
        this.setSelectedItemPath_(menuItem.path);
        return;
      }
    }

    // Nothing is selected.
    this.setSelectedItemPath_('');
  }

  private computeBasicMenuItems_(): MenuItemData[] {
    let basicMenuItems: MenuItemData[];
    if (this.isRevampWayfindingEnabled_) {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          path: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
          sublabel: this.bluetoothMenuItemDescription_,
        },
        {
          section: Section.kMultiDevice,
          path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:connected-devices',
          label: this.i18n('multidevicePageTitle'),
          sublabel: this.multideviceMenuItemDescription_,
        },
        {
          section: Section.kPeople,
          path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'os-settings:account',
          label: this.i18n('osPeoplePageTitle'),
          sublabel: this.accountsMenuItemDescription_,
        },
        {
          section: Section.kKerberos,
          path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          path: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
          sublabel: this.deviceMenuItemDescription_,
        },
        {
          section: Section.kPersonalization,
          path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:personalization',
          label: this.i18n('personalizationPageTitle'),
          sublabel: this.i18n('personalizationMenuItemDescription'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
          sublabel: this.i18n('privacyMenuItemDescription'),
        },
        {
          section: Section.kApps,
          path: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
          icon: 'os-settings:accessibility-revamp',
          label: this.i18n('a11yPageTitle'),
        },
        {
          section: Section.kSystemPreferences,
          path: `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`,
          icon: 'os-settings:system-preferences',
          label: this.i18n('systemPreferencesTitle'),
          sublabel: this.i18n('systemPreferencesMenuItemDescription'),
        },
        {
          section: Section.kAboutChromeOs,
          path: this.aboutMenuItemPath_,
          icon: 'os-settings:chrome',
          label: this.i18n('aboutOsPageTitle'),
          sublabel: this.i18n('aboutChromeOsMenuItemDescription'),
        },
      ];
    } else {
      basicMenuItems = [
        {
          section: Section.kNetwork,
          path: `/${routesMojom.NETWORK_SECTION_PATH}`,
          icon: 'os-settings:network-wifi',
          label: this.i18n('internetPageTitle'),
        },
        {
          section: Section.kBluetooth,
          path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
          icon: 'cr:bluetooth',
          label: this.i18n('bluetoothPageTitle'),
        },
        {
          section: Section.kMultiDevice,
          path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
          icon: 'os-settings:multidevice-better-together-suite',
          label: this.i18n('multidevicePageTitle'),
        },
        {
          section: Section.kPeople,
          path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
          icon: 'cr:person',
          label: this.i18n('osPeoplePageTitle'),
        },
        {
          section: Section.kKerberos,
          path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
          icon: 'os-settings:auth-key',
          label: this.i18n('kerberosPageTitle'),
        },
        {
          section: Section.kDevice,
          path: `/${routesMojom.DEVICE_SECTION_PATH}`,
          icon: 'os-settings:laptop-chromebook',
          label: this.i18n('devicePageTitle'),
        },
        {
          section: Section.kPersonalization,
          path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
          icon: 'os-settings:paint-brush',
          label: this.i18n('personalizationPageTitle'),
        },
        {
          section: Section.kSearchAndAssistant,
          path: `/${routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH}`,
          icon: 'cr:search',
          label: this.i18n('osSearchPageTitle'),
        },
        {
          section: Section.kPrivacyAndSecurity,
          path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
          icon: 'cr:security',
          label: this.i18n('privacyPageTitle'),
        },
        {
          section: Section.kApps,
          path: `/${routesMojom.APPS_SECTION_PATH}`,
          icon: 'os-settings:apps',
          label: this.i18n('appsPageTitle'),
        },
        {
          section: Section.kAccessibility,
          path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
          icon: 'os-settings:accessibility',
          label: this.i18n('a11yPageTitle'),
        },
      ];
    }

    return basicMenuItems.filter(
        ({section}) => !!this.pageAvailability[section]);
  }

  private computeAdvancedMenuItems_(): MenuItemData[] {
    // When OsSettingsRevampWayfinding is enabled, there is no Advanced menu.
    if (this.isRevampWayfindingEnabled_) {
      return [];
    }

    const advancedMenuItems: MenuItemData[] = [
      {
        section: Section.kDateAndTime,
        path: `/${routesMojom.DATE_AND_TIME_SECTION_PATH}`,
        icon: 'os-settings:access-time',
        label: this.i18n('dateTimePageTitle'),
      },
      {
        section: Section.kLanguagesAndInput,
        path: `/${routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH}`,
        icon: 'os-settings:language',
        label: this.i18n('osLanguagesPageTitle'),
      },
      {
        section: Section.kFiles,
        path: `/${routesMojom.FILES_SECTION_PATH}`,
        icon: 'os-settings:folder-outline',
        label: this.i18n('filesPageTitle'),
      },
      {
        section: Section.kPrinting,
        path: `/${routesMojom.PRINTING_SECTION_PATH}`,
        icon: 'os-settings:print',
        label: this.i18n('printingPageTitle'),
      },
      {
        section: Section.kCrostini,
        path: `/${routesMojom.CROSTINI_SECTION_PATH}`,
        icon: 'os-settings:developer-tags',
        label: this.i18n('crostiniPageTitle'),
      },
      {
        section: Section.kReset,
        path: `/${routesMojom.RESET_SECTION_PATH}`,
        icon: 'os-settings:restore',
        label: this.i18n('resetPageTitle'),
      },
    ];

    return advancedMenuItems.filter(
        ({section}) => !!this.pageAvailability[section]);
  }

  private onAdvancedButtonToggle_(): void {
    this.advancedOpened = !this.advancedOpened;
  }

  /**
   * @param path The path of the menu item to be selected. This path should be
   * the pathname portion of a URL, not the full URL. e.g. `/internet`, not
   * `chrome://os-settings/internet`.
   */
  private setSelectedItemPath_(path: string): void {
    this.selectedItemPath_ = path;
  }

  /**
   * Called when a selectable item from <iron-selector> is clicked. This is
   * fired before the selected item is changed.
   */
  private onItemActivated_(event: CustomEvent<{selected: string}>): void {
    this.setSelectedItemPath_(event.detail.selected);
  }

  private onItemSelected_(e: CustomEvent<{item: HTMLElement}>): void {
    e.detail.item.setAttribute('aria-current', 'true');
  }

  private onItemDeselected_(e: CustomEvent<{item: HTMLElement}>): void {
    e.detail.item.removeAttribute('aria-current');
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Which icon to use.
   */
  private arrowState_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }

  /**
   * Updates the "Multidevice" menu item description to one of the following:
   * - If there is a phone connected, show "Connected to <phone name>".
   * - If there is a phone connected but the device name is missing, show
   *   "Connected to Android phone".
   * - If there is no phone connected, show "Phone Hub, Nearby Share".
   */
  private updateMultideviceMenuItemDescription_(
      pageContentData: MultiDevicePageContentData): void {
    if (pageContentData.mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED) {
      if (pageContentData.hostDeviceName) {
        this.multideviceMenuItemDescription_ = this.i18n(
            'multideviceMenuItemDescriptionPhoneConnected',
            pageContentData.hostDeviceName);
      } else {
        this.multideviceMenuItemDescription_ =
            this.i18n('multideviceMenuItemDescriptionDeviceNameMissing');
      }
      return;
    }

    this.multideviceMenuItemDescription_ =
        this.i18n('multideviceMenuItemDescription');
  }

  /**
   * Updates the "Accounts" menu item description to one of the following:
   * - If there are multiple accounts (> 1), show "N accounts".
   * - If there is only one account, show the account email.
   */
  private async updateAccountsMenuItemDescription_(): Promise<void> {
    const accounts =
        await AccountManagerBrowserProxyImpl.getInstance().getAccounts();
    if (accounts.length > 1) {
      this.accountsMenuItemDescription_ =
          this.i18n('accountsMenuItemDescription', accounts.length);
      return;
    }
    const deviceAccount = accounts.find(account => account.isDeviceAccount);
    assertExists(deviceAccount, 'No device account found.');
    this.accountsMenuItemDescription_ = deviceAccount.email;
  }

  private observeBluetoothProperties_(): void {
    this.bluetoothPropertiesObserverReceiver_ =
        new BluetoothPropertiesObserverReceiver(this);
    getBluetoothConfig().observeSystemProperties(
        this.bluetoothPropertiesObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Implements SystemPropertiesObserverInterface */
  onPropertiesUpdated(properties: BluetoothSystemProperties): void {
    const connectedDevices = properties.pairedDevices.filter(
        (device) => device.deviceProperties.connectionState ===
            DeviceConnectionState.kConnected);
    this.updateBluetoothMenuItemDescription_(connectedDevices);
  }

  /**
   * Updates the "Bluetooth" menu item description to one of the following:
   * - No description if no bluetooth devices are connected.
   * - If one device is connected, show the name of the device.
   * - If there are multiple devices connected, show "N devices connected".
   */
  private updateBluetoothMenuItemDescription_(
      connectedDevices: PairedBluetoothDeviceProperties[]): void {
    if (connectedDevices.length === 0) {
      this.bluetoothMenuItemDescription_ = '';
      return;
    }

    if (connectedDevices.length === 1) {
      const device = castExists(connectedDevices[0]);
      this.bluetoothMenuItemDescription_ = getDeviceName(device);
      return;
    }

    this.bluetoothMenuItemDescription_ = this.i18n(
        'bluetoothMenuItemDescriptionMultipleDevicesConnected',
        connectedDevices.length);
  }

  private observeKeyboardSettings_(): void {
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.observeKeyboardSettings(this);
      return;
    }

    this.keyboardSettingsObserverReceiver_ =
        new KeyboardSettingsObserverReceiver(this);
    this.inputDeviceSettingsProvider_.observeKeyboardSettings(
        this.keyboardSettingsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Implements KeyboardSettingsObserverInterface */
  onKeyboardListUpdated(keyboards: Keyboard[]): void {
    this.hasKeyboard_ = keyboards.length > 0;
  }

  /** Implements KeyboardSettingsObserverInterface */
  onKeyboardPoliciesUpdated(): void {
    // Not handled.
  }

  private observeMouseSettings_(): void {
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.observeMouseSettings(this);
      return;
    }

    this.mouseSettingsObserverReceiver_ =
        new MouseSettingsObserverReceiver(this);
    this.inputDeviceSettingsProvider_.observeMouseSettings(
        this.mouseSettingsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Implements MouseSettingsObserverInterface */
  onMouseListUpdated(mice: Mouse[]): void {
    this.hasMouse_ = mice.length > 0;
  }

  /** Implements MouseSettingsObserverInterface */
  onMousePoliciesUpdated(): void {
    // Not handled.
  }

  private observePointingStickSettings_(): void {
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.observePointingStickSettings(this);
      return;
    }

    this.pointingStickSettingsObserverReceiver_ =
        new PointingStickSettingsObserverReceiver(this);
    this.inputDeviceSettingsProvider_.observePointingStickSettings(
        this.pointingStickSettingsObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  /** Implements PointingStickSettingsObserverInterface */
  onPointingStickListUpdated(pointingSticks: PointingStick[]): void {
    this.hasPointingStick_ = pointingSticks.length > 0;
  }

  private observeTouchpadSettings_(): void {
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.observeTouchpadSettings(this);
      return;
    }

    this.touchpadSettingsObserverReceiver_ =
        new TouchpadSettingsObserverReceiver(this);
    this.inputDeviceSettingsProvider_.observeTouchpadSettings(
        this.touchpadSettingsObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** Implements TouchpadSettingsObserverInterface */
  onTouchpadListUpdated(touchpads: Touchpad[]): void {
    this.hasTouchpad_ = touchpads.length > 0;
  }

  /**
   * Only show at most 3 strings in order of priority:
   * - "keyboard" (if available)
   * - "mouse" OR "touchpad" (if available, prioritize mouse)
   * - "print"
   * - "display"
   */
  private computeDeviceMenuItemDescription_(): string {
    if (!this.isRevampWayfindingEnabled_) {
      return '';
    }

    const wordOptions: string[] = [];

    if (this.hasKeyboard_) {
      wordOptions.push(this.i18n('deviceMenuItemDescriptionKeyboard'));
    }

    if (this.hasMouse_ || this.hasPointingStick_) {
      wordOptions.push(this.i18n('deviceMenuItemDescriptionMouse'));
    } else if (this.hasTouchpad_) {
      wordOptions.push(this.i18n('deviceMenuItemDescriptionTouchpad'));
    }

    wordOptions.push(
        this.i18n('deviceMenuItemDescriptionPrint'),
        this.i18n('deviceMenuItemDescriptionDisplay'));

    const words = wordOptions.slice(0, 3);
    return capitalize(words.join(this.i18n('listSeparator')));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsMenuElement.is]: OsSettingsMenuElement;
  }
}

customElements.define(OsSettingsMenuElement.is, OsSettingsMenuElement);

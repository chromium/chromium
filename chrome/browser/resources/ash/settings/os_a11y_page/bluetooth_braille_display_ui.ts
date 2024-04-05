// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A widget that exposes UI for interacting with a list of braille
 * displays.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';

import {BluetoothBrailleDisplayListener, BluetoothBrailleDisplayManager} from './bluetooth_braille_display_manager.js';
import {getTemplate} from './bluetooth_braille_display_ui.html.js';

const CONNECTED_METRIC_NAME =
    'Accessibility.ChromeVox.BluetoothBrailleDisplayConnectedButtonClick';
const PINCODE_TIMEOUT_MS = 60000;
// TODO(b/281743542): Update string for empty braille display picker.
const BLANK_BRAILLE_DISPLAY_MENU_ITEM = {
  value: '',
  name: '',
};

/**
 * A widget used for interacting with bluetooth braille displays.
 * TODO(b/270617362): Add tests for BluetoothBrailleDisplayUi.
 */
const BluetoothBrailleDisplayUiElementBase =
    DeepLinkingMixin(PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export class BluetoothBrailleDisplayUiElement extends
    BluetoothBrailleDisplayUiElementBase implements
        BluetoothBrailleDisplayListener {
  static get is() {
    return 'bluetooth-braille-display-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The braille display dropdown state as a fake preference object.
       */
      brailleDisplayAddressPref_: {
        type: Object,
        observer: 'updateControls_',
        notify: true,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            key: 'BrailleDisplayAddressPref',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: '',
          };
        },
      },

      /**
       * Dropdown menu choices for bluetooth braille display devices.
       */
      brailleDisplayMenuOptions_: {
        type: Array,
        value: [BLANK_BRAILLE_DISPLAY_MENU_ITEM],
      },
    };
  }

  static get observers() {
    return [
      'updateControls_(brailleDisplayMenuOptions_)',
      'updateControls_(brailleDisplayAddressPref_.*)',
    ];
  }

  private brailleDisplayAddressPref_: chrome.settingsPrivate.PrefObject<string>;
  private brailleDisplayMenuOptions_: DropdownMenuOptionList;
  private manager_: BluetoothBrailleDisplayManager;
  private pincodeRequestedDisplay_?: chrome.bluetooth.Device;
  private inPinMode_: boolean = false;
  private pincodeTimeoutId_?: number;
  private selectedDisplay_?: chrome.bluetooth.Device;
  private selectedAndConnectedDisplayAddress_?: string;

  constructor() {
    super();

    this.manager_ = new BluetoothBrailleDisplayManager();
    this.manager_.addListener(this);
  }

  override ready(): void {
    super.ready();
    this.manager_.start();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.manager_.stop();
  }

  onDisplayListChanged(displays: chrome.bluetooth.Device[]): void {
    // If there are no displays, just include a blank menu item.
    if (displays.length === 0) {
      this.brailleDisplayMenuOptions_ = [BLANK_BRAILLE_DISPLAY_MENU_ITEM];
    } else {
      this.brailleDisplayMenuOptions_ = displays.map(display => ({
                                                       value: display.address,
                                                       name: display.name!,
                                                     }));
      // If the blank option was selected, update the display selection.
      if (this.brailleDisplayAddressPref_.value === '') {
        this.set(
            'brailleDisplayAddressPref_.value',
            this.selectedAndConnectedDisplayAddress_ || displays[0].address);
      }
    }
    this.updateControls_();
  }

  private onPincodeChanged_(event: Event): void {
    if (this.pincodeTimeoutId_) {
      clearTimeout(this.pincodeTimeoutId_);
    }

    const pincodeInput = event.target as CrInputElement;
    if (pincodeInput.value) {
      this.manager_.finishPairing(
          this.pincodeRequestedDisplay_!, pincodeInput.value);
    }
    this.inPinMode_ = false;
  }

  onPincodeRequested(display: chrome.bluetooth.Device): void {
    this.inPinMode_ = true;
    this.pincodeRequestedDisplay_ = display;

    // Also, schedule a timeout for pincode entry.
    this.pincodeTimeoutId_ = setTimeout(() => {
      this.inPinMode_ = false;
    }, PINCODE_TIMEOUT_MS);

    // Focus pincode input (after it gets added).
    afterNextRender(this, () => {
      this.shadowRoot!.querySelector<CrInputElement>('#pinCode')!.focus();
    });
  }

  private async updateControls_(): Promise<void> {
    // Only update controls if there is a selected display.
    const selectedDisplayAddress = this.brailleDisplayAddressPref_.value;
    if (!selectedDisplayAddress) {
      this.selectedAndConnectedDisplayAddress_ = undefined;
      return;
    }

    const display = await chrome.bluetooth.getDevice(selectedDisplayAddress);
    this.selectedDisplay_ = display;

    // Record metrics if the display is connected for the first time either
    // via a click of the Connect button or re-connection by selection via the
    // select.
    if (display.connected) {
      if (this.selectedAndConnectedDisplayAddress_ !== selectedDisplayAddress) {
        this.selectedAndConnectedDisplayAddress_ = selectedDisplayAddress;
        chrome.metricsPrivate.recordUserAction(CONNECTED_METRIC_NAME);
      }
    } else {
      // The display is no longer connected.
      if (this.selectedAndConnectedDisplayAddress_ === selectedDisplayAddress) {
        this.selectedAndConnectedDisplayAddress_ = undefined;
      }
    }

    const connectOrDisconnect =
        castExists(this.shadowRoot!.querySelector<CrButtonElement>(
            '#connectOrDisconnect'));
    const displaySelect =
        castExists(this.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#displaySelect'));

    connectOrDisconnect.disabled = display.connecting!;
    displaySelect.disabled = display.connecting!;
    connectOrDisconnect.textContent = loadTimeData.getString(
        display.connecting ?
            'chromeVoxBluetoothBrailleDisplayConnecting' :
            (display.connected ? 'chromeVoxBluetoothBrailleDisplayDisconnect' :
                                 'chromeVoxBluetoothBrailleDisplayConnect'));
    connectOrDisconnect.onclick = () => {
      chrome.metricsPrivate.recordBoolean(
          'ChromeOS.Settings.Accessibility.ConnectBrailleDisplay',
          !display.connected);
      if (display.connected) {
        this.manager_.disconnect(display);
      } else {
        this.manager_.connect(display);
      }
    };

    const forget =
        castExists(this.shadowRoot!.querySelector<CrButtonElement>('#forget'));
    forget.disabled = (!display.paired || display.connecting)!;
    forget.onclick = () => this.manager_.forget(display);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [BluetoothBrailleDisplayUiElement.is]: BluetoothBrailleDisplayUiElement;
  }
}

customElements.define(
    BluetoothBrailleDisplayUiElement.is, BluetoothBrailleDisplayUiElement);

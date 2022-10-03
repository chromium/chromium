// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';
import './input_card.js';
import './keyboard_tester.js';
import './touchscreen_tester.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {ConnectedDevicesObserverReceiver, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo, TouchDeviceType} from './input_data_provider.mojom-webui.js';
import {getTemplate} from './input_list.html.js';
import {KeyboardTesterElement} from './keyboard_tester.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'input-list' is responsible for displaying keyboard, touchpad, and
 * touchscreen cards.
 */

const InputListElementBase = I18nMixin(PolymerElement);

export class InputListElement extends InputListElementBase {
  static get is() {
    return 'input-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      keyboards_: {
        type: Array,
        value: () => [],
      },

      touchpads_: {
        type: Array,
        value: () => [],
      },

      touchscreens_: {
        type: Array,
        value: () => [],
      },

      showTouchpads_: {
        type: Boolean,
        computed: 'computeShowTouchpads_(touchpads_.length)',
      },

      showTouchscreens_: {
        type: Boolean,
        computed: 'computeShowTouchscreens_(touchscreens_.length)',
      },

    };
  }

  protected showTouchpads_: boolean;
  protected showTouchscreens_: boolean;
  private keyboards_: KeyboardInfo[];
  private touchpads_: TouchDeviceInfo[];
  private touchscreens_: TouchDeviceInfo[];
  private connectedDevicesObserverReceiver_: ConnectedDevicesObserverReceiver|
      null = null;
  private keyboardTester: KeyboardTesterElement|null = null;
  private browserProxy_: DiagnosticsBrowserProxy =
      DiagnosticsBrowserProxyImpl.getInstance();
  private inputDataProvider_: InputDataProviderInterface =
      getInputDataProvider();

  private computeShowTouchpads_(numTouchpads: number): boolean {
    return numTouchpads > 0 && loadTimeData.getBoolean('isTouchpadEnabled');
  }

  private computeShowTouchscreens_(numTouchscreens: number): boolean {
    return numTouchscreens > 0 &&
        loadTimeData.getBoolean('isTouchscreenEnabled');
  }

  constructor() {
    super();
    this.browserProxy_.initialize();
    this.loadInitialDevices_();
    this.observeConnectedDevices_();
  }

  private loadInitialDevices_(): void {
    this.inputDataProvider_.getConnectedDevices().then((devices) => {
      this.keyboards_ = devices.keyboards;
      this.touchpads_ = devices.touchDevices.filter(
          (device: TouchDeviceInfo) =>
              device.type === TouchDeviceType.kPointer);
      this.touchscreens_ = devices.touchDevices.filter(
          (device: TouchDeviceInfo) => device.type === TouchDeviceType.kDirect);
    });
  }

  private observeConnectedDevices_(): void {
    this.connectedDevicesObserverReceiver_ =
        new ConnectedDevicesObserverReceiver(this);
    this.inputDataProvider_.observeConnectedDevices(
        this.connectedDevicesObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   */
  onKeyboardConnected(newKeyboard: KeyboardInfo): void {
    this.push('keyboards_', newKeyboard);
  }

  /**
   * Removes the device with the given evdev ID from one of the device list
   * properties.
   * @param path the property's path
   */

  private removeDeviceById_(
      path: 'keyboards_'|'touchpads_'|'touchscreens_', id: number) {
    const index = this.get(path).findIndex(
        (device: KeyboardInfo|TouchDeviceInfo) => device.id === id);
    if (index !== -1) {
      this.splice(path, index, 1);
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardDisconnected.
   */
  onKeyboardDisconnected(id: number): void {
    this.removeDeviceById_('keyboards_', id);
    if (this.keyboards_.length === 0 && this.keyboardTester) {
      // When no keyboards are connected, the <diagnostics-app> component hides
      // the input page. If that happens while a <cr-dialog> is open, the rest
      // of the app remains unresponsive due to the dialog's native logic
      // blocking interaction with other elements. To prevent this we have to
      // explicitly close the dialog when this happens.
      this.keyboardTester.close();
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceConnected.
   */
  onTouchDeviceConnected(newTouchDevice: TouchDeviceInfo): void {
    if (newTouchDevice.type === TouchDeviceType.kPointer) {
      this.push('touchpads_', newTouchDevice);
    } else {
      this.push('touchscreens_', newTouchDevice);
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceDisconnected.
   */
  onTouchDeviceDisconnected(id: number): void {
    this.removeDeviceById_('touchpads_', id);
    this.removeDeviceById_('touchscreens_', id);
  }

  private handleKeyboardTestButtonClick_(e: CustomEvent): void {
    this.keyboardTester = this.shadowRoot!.querySelector('keyboard-tester');
    assert(this.keyboardTester);
    const keyboard: KeyboardInfo|undefined = this.keyboards_.find(
        (keyboard: KeyboardInfo) => keyboard.id === e.detail.evdevId);
    assert(keyboard);
    this.keyboardTester.keyboard = keyboard;
    this.keyboardTester.show();
  }

  /**
   * Handles when the touchscreen Test button is clicked.
   */
  private handleTouchscreenTestButtonClick_(): void {
    const touchscreenTester =
        this.shadowRoot!.querySelector('touchscreen-tester');
    assert(touchscreenTester);
    touchscreenTester.showTester();
  }

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   */
  onNavigationPageChanged({isActive}: {isActive: boolean}): void {
    if (isActive) {
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('input');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-list': InputListElement;
  }
}

customElements.define(InputListElement.is, InputListElement);

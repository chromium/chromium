// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './diagnostics_sticky_banner.js';
import './diagnostics_shared.css.js';
import './input_list.js';
import './network_list.js';
import './strings.m.js';
import './system_page.js';

import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_app.html.js';
import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getDiagnosticsIcon, getNavigationIcon} from './diagnostics_utils.js';
import {ConnectedDevicesObserverReceiver, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

export interface DiagnosticsAppElement {
  $: {
    navigationPanel: NavigationViewPanelElement,
    toast: CrToastElement,
  };
}

// TODO(michaelcheco): Update |InputDataProvider::GetConnectedDevices()| to
// return a |ConnectedDevices| struct instead of defining one here.
interface ConnectedDevices {
  keyboards: KeyboardInfo[];
  touchDevices: TouchDeviceInfo[];
}

/**
 * @fileoverview
 * 'diagnostics-app' is responsible for displaying the 'system-page' which is
 * the main page for viewing telemetric system information and running
 * diagnostic tests.
 */

const DiagnosticsAppElementBase = I18nMixin(PolymerElement);

export class DiagnosticsAppElement extends DiagnosticsAppElementBase {
  static get is() {
    return 'diagnostics-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used in navigation-view-panel to set show-banner when banner is
       * expected to be shown.
       */
      bannerMessage_: {
        type: Boolean,
        value: '',
      },

      saveSessionLogEnabled_: {
        type: Boolean,
        value: true,
      },

      isInputEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isInputEnabled'),
      },

      /**
       * Whether a user is logged in or not.
       * Note: A guest session is considered a logged-in state.
       */
      isLoggedIn_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      toastText_: {
        type: String,
        value: '',
      },
    };
  }

  protected bannerMessage_: string;
  protected isLoggedIn_: boolean;
  private saveSessionLogEnabled_: boolean;
  private isInputEnabled_: boolean;
  private toastText_: string;
  private browserProxy_: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();
  private inputDataProvider_: InputDataProviderInterface =
      getInputDataProvider();
  private numKeyboards_: number = 0;

  constructor() {
    super();
    this.browserProxy_.initialize();
    if (this.isInputEnabled_) {
      this.inputDataProvider_.observeConnectedDevices(
          new ConnectedDevicesObserverReceiver(this)
              .$.bindNewPipeAndPassRemote());
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   */
  onKeyboardConnected(): void {
    this.numKeyboards_++;
    // Note: This will need to be revisited if additional navigation pages are
    // created as the navigation panel may have to be updated to ensure pages
    // appear in the correct order.
    if (!this.$.navigationPanel.pageExists('input')) {
      this.$.navigationPanel.addSelectorItem(this.createInputSelector());
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardDisconnected.
   */
  onKeyboardDisconnected(): void {
    this.numKeyboards_--;
    if (this.numKeyboards_ === 0) {
      this.$.navigationPanel.removeSelectorById('input');
    }
  }

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceConnected.
   */
  onTouchDeviceConnected(): void {}

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceDisconnected.
   */
  onTouchDeviceDisconnected(): void {}

  // Note: When adding a new page, update the DiagnosticsPage enum located
  // in chrome/browser/ui/webui/ash/diagnostics_dialog.h.
  private async getNavPages(): Promise<SelectorItem[]> {
    const pages: SelectorItem[] = [
      this.$.navigationPanel.createSelectorItem(
          loadTimeData.getString('systemText'), 'system-page',
          getNavigationIcon('laptop-chromebook'), 'system'),
      this.$.navigationPanel.createSelectorItem(
          loadTimeData.getString('connectivityText'), 'network-list',
          getNavigationIcon('ethernet'), 'connectivity'),
    ];

    if (this.isInputEnabled_) {
      const devices: ConnectedDevices =
          await this.inputDataProvider_.getConnectedDevices();
      // Check the existing value of |numKeyboards_| if |GetConnectedDevices|
      // returns no keyboards as it's possible |onKeyboardConnected| was called
      // prior.
      this.numKeyboards_ = devices.keyboards.length || this.numKeyboards_;
      const isTouchPadOrTouchScreenEnabled =
          loadTimeData.getBoolean('isTouchpadEnabled') ||
          loadTimeData.getBoolean('isTouchscreenEnabled');
      if (this.numKeyboards_ > 0 || isTouchPadOrTouchScreenEnabled) {
        pages.push(this.createInputSelector());
      }
    }

    return pages;
  }

  private async createNavigationPanel(): Promise<void> {
    this.$.navigationPanel.addSelectors(await this.getNavPages());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.createNavigationPanel();
  }

  protected onSessionLogClick_(): void {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled_) {
      return;
    }

    this.saveSessionLogEnabled_ = false;
    this.browserProxy_.saveSessionLog()
        .then((success: boolean) => {
          const result = success ? 'Success' : 'Failure';
          this.toastText_ =
              loadTimeData.getString(`sessionLogToastText${result}`);
          this.$.toast.show();
        })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled_ = true;
        });
  }

  // Note: addSelectorItem or addSelectors still needs to be called to add
  // the input page to the navigation panel.
  private createInputSelector(): SelectorItem {
    return this.$.navigationPanel.createSelectorItem(
        loadTimeData.getString('inputText'), 'input-list',
        getDiagnosticsIcon('keyboard'), 'input');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-app': DiagnosticsAppElement;
  }
}

customElements.define(DiagnosticsAppElement.is, DiagnosticsAppElement);

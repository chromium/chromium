// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './diagnostics_sticky_banner.js';
import './diagnostics_shared.css.js';
import './input_list.js';
import './network_list.js';
import './strings.m.js';
import './system_page.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_app.html.js';
import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getDiagnosticsIcon, getNavigationIcon} from './diagnostics_utils.js';
import {KeyboardInfo} from './input.mojom-webui.js';
import {ConnectedDevicesObserverReceiver, InputDataProviderInterface, TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

export interface DiagnosticsAppElement {
  $: {
    navigationPanel: NavigationViewPanelElement,
    toast: CrToastElement,
  };
}

export type ShowToastEvent = CustomEvent<{message: string}>;

declare global {
  interface HTMLElementEventMap {
    'show-toast': ShowToastEvent;
  }
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
  static get is(): string {
    return 'diagnostics-app';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Used in navigation-view-panel to set show-banner when banner is
       * expected to be shown.
       */
      bannerMessage: {
        type: Boolean,
        value: '',
      },

      saveSessionLogEnabled: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether a user is logged in or not.
       * Note: A guest session is considered a logged-in state.
       */
      isLoggedIn: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      toastText: {
        type: String,
        value: '',
      },
    };
  }

  protected bannerMessage: string;
  protected isLoggedIn: boolean;
  private saveSessionLogEnabled: boolean;
  private toastText: string;
  private browserProxy: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();
  private inputDataProvider: InputDataProviderInterface =
      getInputDataProvider();
  private numKeyboards: number = 0;

  constructor() {
    super();
    this.browserProxy.initialize();
    this.inputDataProvider.observeConnectedDevices(
        new ConnectedDevicesObserverReceiver(this)
            .$.bindNewPipeAndPassRemote());
  }

  /**
   * Event callback for 'show-toast' which is triggered from input-list. Event
   * will contain message to display on message property of event found on
   * event found on path `e.detail.message`.
   */
  private showToastHandler = (e: ShowToastEvent): void => {
    assert(e.detail.message);
    this.toastText = e.detail.message;
    this.$.toast.show();
  };

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   */
  onKeyboardConnected(): void {
    this.numKeyboards++;
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
    this.numKeyboards--;
    if (this.numKeyboards === 0) {
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
  // in chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h.
  private async getNavPages(): Promise<SelectorItem[]> {
    const pages: SelectorItem[] = [
      this.$.navigationPanel.createSelectorItem(
          loadTimeData.getString('systemText'), 'system-page',
          getNavigationIcon('laptop-chromebook'), 'system'),
      this.$.navigationPanel.createSelectorItem(
          loadTimeData.getString('connectivityText'), 'network-list',
          getNavigationIcon('ethernet'), 'connectivity'),
    ];

    pages.push(this.createInputSelector());
    const devices: ConnectedDevices =
        await this.inputDataProvider.getConnectedDevices();
    // Check the existing value of |numKeyboards| if |GetConnectedDevices|
    // returns no keyboards as it's possible |onKeyboardConnected| was called
    // prior.
    this.numKeyboards = devices.keyboards.length || this.numKeyboards;
    const isTouchPadOrTouchScreenEnabled =
        loadTimeData.getBoolean('isTouchpadEnabled') ||
        loadTimeData.getBoolean('isTouchscreenEnabled');
    if (this.numKeyboards === 0 && !isTouchPadOrTouchScreenEnabled) {
      pages.pop();
    }

    return pages;
  }

  private async createNavigationPanel(): Promise<void> {
    this.$.navigationPanel.addSelectors(await this.getNavPages());
  }

  override connectedCallback(): void {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();

    this.createNavigationPanel();
    window.addEventListener(
        'show-toast', (e) => this.showToastHandler((e as ShowToastEvent)));
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener(
        'show-toast', (e) => this.showToastHandler((e as ShowToastEvent)));
  }

  protected onSessionLogClick(): void {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled) {
      return;
    }

    this.saveSessionLogEnabled = false;
    this.browserProxy.saveSessionLog()
        .then((success: boolean) => {
          const result = success ? 'Success' : 'Failure';
          this.toastText =
              loadTimeData.getString(`sessionLogToastText${result}`);
          this.$.toast.show();
        })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled = true;
        });
  }

  // Note: addSelectorItem or addSelectors still needs to be called to add
  // the input page to the navigation panel.
  private createInputSelector(): SelectorItem {
    return this.$.navigationPanel.createSelectorItem(
        loadTimeData.getString('keyboardText'), 'input-list',
        getDiagnosticsIcon('keyboard'), 'input');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-app': DiagnosticsAppElement;
  }
}

customElements.define(DiagnosticsAppElement.is, DiagnosticsAppElement);

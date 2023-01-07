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
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_app.html.js';
import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getDiagnosticsIcon, getNavigationIcon} from './diagnostics_utils.js';
import {ConnectedDevicesObserverReceiver, InputDataProviderInterface} from './input_data_provider.mojom-webui.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

export interface DiagnosticsAppElement {
  $: {
    navigationPanel: NavigationViewPanelElement,
    toast: CrToastElement,
  };
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

      showNavPanel_: {
        type: Boolean,
        computed: 'computeShowNavPanel_(isNetworkingEnabled_, isInputEnabled_)',
      },

      isNetworkingEnabled_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isNetworkingEnabled'),
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
  private showNavPanel_: boolean;
  private isNetworkingEnabled_: boolean;
  private isInputEnabled_: boolean;
  private toastText_: string;
  private browserProxy_: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();
  private inputDataProvider_: InputDataProviderInterface|null = null;
  private numKeyboards_: number = -1;

  constructor() {
    super();
    this.browserProxy_.initialize();
  }

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   */
  onKeyboardConnected(): void {
    if (this.numKeyboards_ === 0) {
      this.$.navigationPanel.addSelectorItem(this.createInputSelector_());
    }
    this.numKeyboards_++;
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

  private computeShowNavPanel_(
      isNetworkingEnabled: boolean, isInputEnabled: boolean): boolean {
    return isNetworkingEnabled || isInputEnabled;
  }

  private createInputSelector_(): SelectorItem {
    return this.$.navigationPanel.createSelectorItem(
        loadTimeData.getString('inputText'), 'input-list',
        getDiagnosticsIcon('keyboard'), 'input');
  }

  override connectedCallback() {
    super.connectedCallback();

    if (this.showNavPanel_) {
      const navPanel: NavigationViewPanelElement|null =
          this.shadowRoot!.querySelector('#navigationPanel');
      assert(navPanel);
      // Note: When adding a new page, update the DiagnosticsPage enum located
      // in chrome/browser/ui/webui/chromeos/diagnostics_dialog.h.
      const pages: SelectorItem[] = [navPanel.createSelectorItem(
          loadTimeData.getString('systemText'), 'system-page',
          getNavigationIcon('laptop-chromebook'), 'system')];

      if (this.isNetworkingEnabled_) {
        pages.push(navPanel.createSelectorItem(
            loadTimeData.getString('connectivityText'), 'network-list',
            getNavigationIcon('ethernet'), 'connectivity'));
      }

      if (this.isInputEnabled_) {
        if (loadTimeData.getBoolean('isTouchpadEnabled') ||
            loadTimeData.getBoolean('isTouchscreenEnabled')) {
          pages.push(this.createInputSelector_());
        } else {
          // We only want to show the Input page in the selector if one or more
          // (testable) keyboards are present.
          this.inputDataProvider_ = getInputDataProvider();
          this.inputDataProvider_.getConnectedDevices().then((devices) => {
            this.numKeyboards_ = devices.keyboards.length;
            if (this.numKeyboards_ > 0) {
              navPanel.addSelectorItem(this.createInputSelector_());
            }
          });
          const receiver = new ConnectedDevicesObserverReceiver(
              /** @type {!ConnectedDevicesObserverInterface} */ (this));
          this.inputDataProvider_.observeConnectedDevices(
              receiver.$.bindNewPipeAndPassRemote());
        }
      }
      navPanel.addSelectors(pages);
    }
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
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-app': DiagnosticsAppElement;
  }
}

customElements.define(DiagnosticsAppElement.is, DiagnosticsAppElement);

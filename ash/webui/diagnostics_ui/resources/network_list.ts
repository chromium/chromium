// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './connectivity_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './network_card.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ConnectivityCardElement} from './connectivity_card.js';
import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {NetworkCardElement} from './network_card.js';
import {NetworkHealthProviderInterface, NetworkListObserverReceiver} from './network_health_provider.mojom-webui.js';
import {getTemplate} from './network_list.html.js';
import {TestSuiteStatus} from './routine_list_executor.js';

export interface NetworkListElement {
  $: {
    networkListContainer: HTMLDivElement,
  };
}

/**
 * @fileoverview
 * 'network-list' is responsible for displaying Ethernet, Cellular,
 *  and WiFi networks.
 */

const NetworkListElementBase = I18nMixin(PolymerElement);

export class NetworkListElement extends NetworkListElementBase {
  static get is() {
    return 'network-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
      },

      otherNetworkGuids_: {
        type: Array,
        value: () => [],
      },

      activeGuid_: {
        type: String,
        value: '',
      },

      isActive: {
        type: Boolean,
        value: true,
      },

      isLoggedIn_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  isActive: boolean;
  protected isLoggedIn_: boolean;
  private otherNetworkGuids_: string[];
  private activeGuid_: string;
  private browserProxy_: DiagnosticsBrowserProxy =
      DiagnosticsBrowserProxyImpl.getInstance();
  private networkHealthProvider_: NetworkHealthProviderInterface =
      getNetworkHealthProvider();
  private networkListObserverReceiver_: NetworkListObserverReceiver|null = null;

  constructor() {
    super();
    this.browserProxy_.initialize();
    this.observeNetworkList_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.networkListObserverReceiver_) {
      this.networkListObserverReceiver_.$.close();
    }
  }

  private observeNetworkList_(): void {
    // Calling observeNetworkList will trigger onNetworkListChanged.
    this.networkListObserverReceiver_ = new NetworkListObserverReceiver(this);

    this.networkHealthProvider_.observeNetworkList(
        this.networkListObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements NetworkListObserver.onNetworkListChanged
   */
  onNetworkListChanged(networkGuids: string[], activeGuid: string): void {
    // The connectivity-card is responsible for displaying the active network
    // so we need to filter out the activeGuid to avoid displaying a
    // a network-card for it.
    this.otherNetworkGuids_ = networkGuids.filter(guid => guid !== activeGuid);
    this.activeGuid_ = activeGuid;
  }

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   */
  onNavigationPageChanged({isActive}: {isActive: boolean}): void {
    this.isActive = isActive;
    // TODO(ashleydp): Update when connectivity/network card's are merged.
    if (isActive) {
      // Focus the first visible card title. If no cards are present,
      // fallback to focusing the element's main container.
      afterNextRender(this, () => {
        if (this.activeGuid_) {
          const connectivityCard: ConnectivityCardElement|null =
              this.shadowRoot!.querySelector('connectivity-card');
          assert(connectivityCard);
          const cardTitle: HTMLDivElement|null =
              connectivityCard.shadowRoot!.querySelector('#cardTitle');
          assert(cardTitle);
          cardTitle.focus();
          return;
        } else if (this.otherNetworkGuids_.length > 0) {
          const networkCard: NetworkCardElement|null =
              this.shadowRoot!.querySelector('network-card');
          assert(networkCard);
          const cardTitle: HTMLDivElement|null =
              networkCard.shadowRoot!.querySelector('#cardTitle');
          assert(cardTitle);
          cardTitle.focus();
        }
        this.$.networkListContainer.focus();
      });
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('connectivity');
    }
  }

  protected getSettingsString_(): TrustedHTML {
    return this.i18nAdvanced('settingsLinkText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'network-list': NetworkListElement;
  }
}

customElements.define(NetworkListElement.is, NetworkListElement);

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getSubnetMaskFromRoutingPrefix} from './diagnostics_utils.js';
import {getTemplate} from './ip_config_info_drawer.html.js';
import {Network} from './network_health_provider.mojom-webui.js';

/**
 * @fileoverview
 * 'ip-config-info-drawer' displays standard IP related configuration data in a
 * collapsible drawer.
 */

const IpConfigInfoDrawerElementBase = I18nMixin(PolymerElement);

export class IpConfigInfoDrawerElement extends IpConfigInfoDrawerElementBase {
  static get is(): string {
    return 'ip-config-info-drawer';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      expanded: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      gateway: {
        type: String,
        computed: 'computeGateway(network.ipConfig.gateway)',
      },

      nameServers: {
        type: String,
        computed: 'computeNameServers(network.ipConfig.nameServers)',
      },

      network: {
        type: Object,
      },

      subnetMask: {
        type: String,
        computed: 'computeSubnetMask(network.ipConfig.routingPrefix)',
      },

      nameServersHeader: {
        type: String,
        value: '',
      },
    };
  }

  network: Network;
  protected expanded: boolean;
  protected gateway: string;
  protected nameServers: string;
  protected subnetMask: string;
  protected nameServersHeader: string;
  private browserProxy: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();

  static get observers(): string[] {
    return ['getNameServersHeader(network.ipConfig.nameServers)'];
  }

  protected computeGateway(): string {
    return this.network?.ipConfig?.gateway || '';
  }

  protected computeNameServers(): string {
    if (!this.network?.ipConfig) {
      return '';
    }

    // Handle name servers null or zero length state.
    if (!this.network.ipConfig?.nameServers ||
        this.network.ipConfig.nameServers?.length === 0) {
      return loadTimeData.getStringF('networkDnsNotConfigured');
    }

    return this.network.ipConfig.nameServers.join(', ');
  }

  protected computeSubnetMask(): string {
    // Routing prefix should be [1,32] when set. 0 indicates an unset value.
    if (this.network?.ipConfig && this.network?.ipConfig?.routingPrefix &&
        this.network?.ipConfig?.routingPrefix >= 0 &&
        this.network?.ipConfig?.routingPrefix <= 32) {
      return getSubnetMaskFromRoutingPrefix(
          this.network?.ipConfig?.routingPrefix);
    }
    return '';
  }

  protected getNameServersHeader(nameServers?: string[]): void {
    const count = nameServers ? nameServers.length : 0;
    this.browserProxy.getPluralString('nameServersText', count)
        .then((localizedString: string) => {
          this.nameServersHeader = localizedString;
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ip-config-info-drawer': IpConfigInfoDrawerElement;
  }
}

customElements.define(IpConfigInfoDrawerElement.is, IpConfigInfoDrawerElement);

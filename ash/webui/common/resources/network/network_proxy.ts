// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './network_proxy_exclusions.js';
import './network_proxy_input.js';
import './network_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ManagedManualProxySettings, ManagedProperties, ManagedProxyLocation, ManagedProxySettings, ManagedStringList, ManualProxySettings, ProxyLocation, ProxySettings} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType, OncSource, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {microTask, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_proxy.html.js';
import {OncMojo} from './onc_mojo.js';

function createDefaultProxySettings(): ManagedProxySettings {
  return {
    type: OncMojo.createManagedString('Direct'),
    manual: undefined,
    excludeDomains: undefined,
    pac: undefined,
  };
}

type Constructor<T> = new (...args: any[]) => T;

const NetworkProxyElementBase =
    mixinBehaviors([CrPolicyNetworkBehaviorMojo], I18nMixin(PolymerElement)) as
    Constructor<PolymerElement&I18nMixinInterface&
                CrPolicyNetworkBehaviorMojoInterface>;

export class NetworkProxyElement extends NetworkProxyElementBase {
  static get is() {
    return 'network-proxy' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether or not the proxy values can be edited. */
      editable: {
        type: Boolean,
        value: false,
      },

      managedProperties: {
        type: Object,
        observer: 'managedPropertiesChanged_',
      },

      /** Whether shared proxies are allowed. */
      useSharedProxies: {
        type: Boolean,
        value: false,
        observer: 'updateProxy_',
      },

      /**
       * UI visible / edited proxy configuration.
       */
      proxy_: {
        type: Object,
        value() {
          return createDefaultProxySettings();
        },
      },

      /**
       * The Web Proxy Auto Discovery URL extracted from managedProperties.
       */
      wpad_: {
        type: String,
        value: '',
      },

      /**
       * Whether or not to use the same manual proxy for all protocols.
       */
      useSameProxy_: {
        type: Boolean,
        value: false,
        observer: 'useSameProxyChanged_',
      },

      /**
       * Array of proxy configuration types.
       */
      proxyTypes_: {
        type: Array,
        value: ['Direct', 'PAC', 'WPAD', 'Manual'],
        readOnly: true,
      },

      /**
       * The current value of the proxy exclusion input.
       */
      proxyExclusionInputValue_: {
        type: String,
        value: '',
      },

      /**
       * Set to true while modifying proxy values so that an update does not
       * override the edited values.
       */
      proxyIsUserModified_: {
        type: Boolean,
        value: false,
      },

    };
  }

  editable: boolean;
  managedProperties: ManagedProperties|undefined;
  useSharedProxies: boolean;
  private proxy_: ManagedProxySettings;
  private wpad_: string;
  private useSameProxy_: boolean;
  private proxyTypes_: [];
  private proxyExclusionInputValue_: string;
  private proxyIsUserModified_: boolean;

  /**
   * Saved ExcludeDomains properties so that switching to a non-Manual type
   * does not loose any set exclusions while the UI is open.
   */
  private savedManual_: ManagedManualProxySettings|undefined = undefined;

  /**
   * Saved Manual properties so that switching to another type does not loose
   * any set properties while the UI is open.
   */
  private savedExcludeDomains_: ManagedStringList|undefined = undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.reset();
  }

  /**
   * Called any time the page is refreshed or navigated to so that the proxy
   * is updated correctly.
   */
  reset() {
    this.proxyIsUserModified_ = false;
    this.updateProxy_();
  }

  private managedPropertiesChanged_(
      newValue: ManagedProperties|undefined,
      oldValue: ManagedProperties|undefined) {
    if ((newValue && newValue.guid) !== (oldValue && oldValue.guid)) {
      // Clear saved manual properties and exclude domains if we're updating
      // to show a different network.
      this.savedManual_ = undefined;
      this.savedExcludeDomains_ = undefined;
    }

    if (this.proxyIsUserModified_ || this.isInputEditInProgress_()) {
      // Ignore updates if any fields have been modified by user or if any
      // input elements are currently being edited.
      return;
    }
    this.updateProxy_();
  }

  private isInputEditInProgress_(): boolean {
    if (!this.editable) {
      return false;
    }
    const activeElement = this.shadowRoot!.activeElement;
    if (!activeElement) {
      return false;
    }

    // Find property name for current active element.
    let property = null;
    switch (activeElement.id) {
      case 'sameProxyInput':
      case 'httpProxyInput':
        property = 'manual.httpProxy.host';
        break;
      case 'secureHttpProxyInput':
        property = 'manual.secureHttpProxy.host';
        break;
      case 'socksProxyInput':
        property = 'manual.socks.host';
        break;
      case 'pacInput':
        property = 'pac';
        break;
    }
    if (!property) {
      return false;
    }

    // Input should be considered active only when the property editable.
    return this.isEditable_(property);
  }

  private proxyMatches_(
      a: ManagedProxyLocation|undefined|null,
      b: ManagedProxyLocation|undefined|null): boolean {
    return !!a && !!b && a.host.activeValue === b.host.activeValue &&
        a.port.activeValue === b.port.activeValue;
  }

  private createDefaultProxyLocation_(port: number): ManagedProxyLocation {
    return {
      host: OncMojo.createManagedString(''),
      port: OncMojo.createManagedInt(port),
    };
  }

  /**
   * Returns a copy of |inputProxy| with all required properties set correctly.
   */
  private validateProxy_(inputProxy: ManagedProxySettings):
      ManagedProxySettings {
    const proxy = {...inputProxy};
    const type = proxy.type.activeValue;
    if (type === 'PAC') {
      if (!proxy.pac) {
        proxy.pac = OncMojo.createManagedString('');
      }
    } else if (type === 'Manual') {
      proxy.manual =
          proxy.manual || this.savedManual_ || new ManagedManualProxySettings();
      assert(proxy.manual);
      if (!proxy.manual.httpProxy) {
        proxy.manual.httpProxy = this.createDefaultProxyLocation_(80);
      }
      if (!proxy.manual.secureHttpProxy) {
        proxy.manual.secureHttpProxy = this.createDefaultProxyLocation_(80);
      }
      if (!proxy.manual.socks) {
        proxy.manual.socks = this.createDefaultProxyLocation_(1080);
      }
      proxy.excludeDomains =
          proxy.excludeDomains || this.savedExcludeDomains_ || {
            activeValue: [],
            policySource: PolicySource.kNone,
            policyValue: undefined,
          };
    }
    return proxy;
  }

  private updateProxy_(): void {
    if (!this.managedProperties) {
      return;
    }

    let proxySettings = this.managedProperties.proxySettings;

    // For shared networks with unmanaged proxy settings, ignore any saved proxy
    // settings and use the default value.
    if (this.isShared_() && proxySettings &&
        !this.isControlled(proxySettings.type) && !this.useSharedProxies) {
      proxySettings = undefined;  // Ignore proxy settings.
    }

    const proxy = proxySettings ? this.validateProxy_(proxySettings) :
                                  createDefaultProxySettings();

    if (proxy.type.activeValue === 'WPAD') {
      // Set the Web Proxy Auto Discovery URL for display purposes.
      const ipv4 = this.managedProperties ?
          OncMojo.getIPConfigForType(
              this.managedProperties, IPConfigType.kIPv4) :
          null;
      this.wpad_ = (ipv4 && ipv4.webProxyAutoDiscoveryUrl) ||
          this.i18n('networkProxyWpadNone');
    }

    // Set this.proxy_ after dom-repeat has been stamped.
    microTask.run(() => this.setProxy_(proxy));
  }

  private setProxy_(proxy: ManagedProxySettings): void {
    this.proxy_ = proxy;
    if (proxy.manual) {
      const manual = proxy.manual;
      const httpProxy = manual.httpProxy;
      if (this.proxyMatches_(httpProxy, manual.secureHttpProxy) &&
          this.proxyMatches_(httpProxy, manual.socks)) {
        // If all four proxies match, enable the 'use same proxy' toggle.
        this.useSameProxy_ = true;
      } else if (
          !manual.secureHttpProxy?.host?.activeValue &&
          !manual.socks?.host?.activeValue) {
        // Otherwise if no proxies other than http have a host value, also
        // enable the 'use same proxy' toggle.
        this.useSameProxy_ = true;
      }
    }
    this.proxyIsUserModified_ = false;
  }

  private useSameProxyChanged_(): void {
    this.proxyIsUserModified_ = true;
  }

  private getProxyLocation_(location: ManagedProxyLocation|undefined|
                            null): ProxyLocation|undefined {
    if (!location) {
      return undefined;
    }
    return {
      host: location.host.activeValue,
      port: location.port.activeValue,
    };
  }

  /**
   * Called when the proxy changes in the UI.
   */
  private sendProxyChange_(): void {
    const proxyType = OncMojo.getActiveString(this.proxy_.type);
    if (!proxyType || (proxyType === 'PAC' && !this.proxy_.pac)) {
      return;
    }

    const proxy = new ProxySettings();
    proxy.type = proxyType;
    proxy.excludeDomains =
        OncMojo.getActiveValue(this.proxy_.excludeDomains) as string[] |
        undefined;

    if (proxyType === 'Manual') {
      let manual = new ManualProxySettings();
      if (this.proxy_.manual) {
        this.savedManual_ = {...this.proxy_.manual};
        manual = {
          httpProxy: this.getProxyLocation_(this.proxy_.manual.httpProxy),
          secureHttpProxy:
              this.getProxyLocation_(this.proxy_.manual.secureHttpProxy),
          ftpProxy: undefined,
          socks: this.getProxyLocation_(this.proxy_.manual.socks),
        };
      }
      if (this.proxy_.excludeDomains) {
        this.savedExcludeDomains_ = {...this.proxy_.excludeDomains};
      }
      const defaultProxy = manual.httpProxy || {host: '', port: 80};
      if (this.useSameProxy_) {
        manual.secureHttpProxy = {...defaultProxy};
        manual.socks = {...defaultProxy};
      } else {
        // Remove properties with empty hosts to unset them.
        if (manual.httpProxy && !manual.httpProxy.host) {
          delete manual.httpProxy;
        }
        if (manual.secureHttpProxy && !manual.secureHttpProxy.host) {
          delete manual.secureHttpProxy;
        }
        if (manual.socks && !manual.socks.host) {
          delete manual.socks;
        }
      }
      proxy.manual = manual;
    } else if (proxyType === 'PAC') {
      proxy.pac = OncMojo.getActiveString(this.proxy_.pac);
    }
    this.dispatchEvent(new CustomEvent('proxy-change', {
      bubbles: true,
      composed: true,
      detail: proxy,
    }));
    this.proxyIsUserModified_ = false;
  }

  /**
   * Event triggered when the selected proxy type changes.
   */
  private onTypeChange_(event: Event): void {
    if (!this.proxy_ || !this.proxy_.type) {
      return;
    }
    const target = event.target as HTMLSelectElement;
    const type = target.value;
    this.proxy_.type.activeValue = type;
    this.set('proxy_', this.validateProxy_(this.proxy_));
    let proxyTypeChangeIsReady;
    let elementToFocus;
    switch (type) {
      case 'Direct':
      case 'WPAD':
        // No addtional values are required, send the type change.
        proxyTypeChangeIsReady = true;
        break;
      case 'PAC':
        elementToFocus = this.shadowRoot!.querySelector('#pacInput');
        // If a PAC is already set, send the type change now, otherwise wait
        // until the user provides a PAC value.
        proxyTypeChangeIsReady = !!OncMojo.getActiveString(this.proxy_.pac);
        break;
      case 'Manual':
        // Manual proxy configuration includes multiple input fields, so wait
        // until the 'send' button is clicked.
        proxyTypeChangeIsReady = false;
        elementToFocus =
            this.shadowRoot!.querySelector('#manualProxy network-proxy-input');
        break;
    }

    // If the new proxy type is fully configured, send it, otherwise set
    // |proxyIsUserModified_| to true so that property updates do not
    // overwrite user changes.
    if (proxyTypeChangeIsReady) {
      this.sendProxyChange_();
    } else {
      this.proxyIsUserModified_ = true;
    }

    if (elementToFocus) {
      microTask.run(() => elementToFocus.focus());
    }
  }

  private onPacChange_(): void {
    this.sendProxyChange_();
  }

  private onProxyInputChange_(): void {
    this.proxyIsUserModified_ = true;
  }

  private onAddProxyExclusionClicked_(): void {
    assert(this.proxyExclusionInputValue_);
    this.push(
        'proxy_.excludeDomains.activeValue', this.proxyExclusionInputValue_);
    // Clear input.
    this.proxyExclusionInputValue_ = '';
    this.proxyIsUserModified_ = true;
  }

  private onAddProxyExclusionKeypress_(event: KeyboardEvent): void {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.onAddProxyExclusionClicked_();
  }

  private shouldProxyExclusionButtonBeDisabled_(proxyExclusionInputValue:
                                                    string): boolean {
    return !proxyExclusionInputValue;
  }

  /**
   * Event triggered when the proxy exclusion list changes.
   */
  private onProxyExclusionsChange_(): void {
    this.proxyIsUserModified_ = true;
  }

  private onSaveProxyClicked_(): void {
    this.sendProxyChange_();
  }

  private getProxyTypeDesc_(proxyType: string): string {
    if (proxyType === 'Manual') {
      return this.i18n('networkProxyTypeManual');
    }
    if (proxyType === 'PAC') {
      return this.i18n('networkProxyTypePac');
    }
    if (proxyType === 'WPAD') {
      return this.i18n('networkProxyTypeWpad');
    }
    return this.i18n('networkProxyTypeDirect');
  }

  private isEditable_(propertyName: string): boolean {
    if (!this.editable || (this.isShared_() && !this.useSharedProxies)) {
      return false;
    }
    const property =
        this.get('proxySettings.' + propertyName, this.managedProperties);
    if (!property) {
      return true;  // Default to editable if property is not defined.
    }
    return this.isPropertyEditable_(property);
  }

  private isPropertyEditable_(property: OncMojo.ManagedProperty|
                              undefined): boolean {
    return !!property && !this.isNetworkPolicyEnforced(property) &&
        !this.isExtensionControlled(property);
  }

  private isShared_(): boolean {
    if (!this.managedProperties) {
      return false;
    }
    const source = this.managedProperties.source;
    return source === OncSource.kDevice || source === OncSource.kDevicePolicy;
  }

  private isSaveManualProxyEnabled_(): boolean {
    if (!this.proxyIsUserModified_) {
      return false;
    }
    const manual = this.proxy_.manual;
    const httpHost = this.get('httpProxy.host.activeValue', manual);
    if (this.useSameProxy_) {
      return !!httpHost;
    }
    return !!httpHost ||
        !!this.get('secureHttpProxy.host.activeValue', manual) ||
        !!this.get('socks.host.activeValue', manual);
  }

  private matches_(property: string, value: string): boolean {
    return property === value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkProxyElement.is]: NetworkProxyElement;
  }
}

customElements.define(NetworkProxyElement.is, NetworkProxyElement);

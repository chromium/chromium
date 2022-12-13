// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './network_proxy_exclusions.js';
import './network_proxy_input.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {assert} from '//resources/ash/common/assert.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManagedManualProxySettings, ManagedProperties, ManagedProxyLocation, ManagedProxySettings, ManagedStringList, ProxyLocation, ProxySettings} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType, OncSource, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_proxy.html.js';
import {OncMojo} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-proxy',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
  ],

  properties: {
    /** Whether or not the proxy values can be edited. */
    editable: {
      type: Boolean,
      value: false,
    },

    /** @type {!ManagedProperties|undefined} */
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
     * @private {!ManagedProxySettings}
     */
    proxy_: {
      type: Object,
      value() {
        return this.createDefaultProxySettings_();
      },
    },

    /**
     * The Web Proxy Auto Discovery URL extracted from managedProperties.
     * @private
     */
    wpad_: {
      type: String,
      value: '',
    },

    /**
     * Whether or not to use the same manual proxy for all protocols.
     * @private
     */
    useSameProxy_: {
      type: Boolean,
      value: false,
      observer: 'useSameProxyChanged_',
    },

    /**
     * Array of proxy configuration types.
     * @private {!Array<string>}
     * @const
     */
    proxyTypes_: {
      type: Array,
      value: ['Direct', 'PAC', 'WPAD', 'Manual'],
      readOnly: true,
    },

    /**
     * The current value of the proxy exclusion input.
     * @private
     */
    proxyExclusionInputValue_: {
      type: String,
      value: '',
    },
  },

  /**
   * Saved Manual properties so that switching to another type does not loose
   * any set properties while the UI is open.
   * @private {!ManagedManualProxySettings|
   *           undefined}
   */
  savedManual_: undefined,

  /**
   * Saved ExcludeDomains properties so that switching to a non-Manual type does
   * not loose any set exclusions while the UI is open.
   * @private {!ManagedStringList|undefined}
   */
  savedExcludeDomains_: undefined,

  /**
   * Set to true while modifying proxy values so that an update does not
   * override the edited values.
   * @private {boolean}
   */
  proxyIsUserModified_: false,

  /** @override */
  attached() {
    this.reset();
  },

  /**
   * Called any time the page is refreshed or navigated to so that the proxy
   * is updated correctly.
   */
  reset() {
    this.proxyIsUserModified_ = false;
    this.updateProxy_();
  },

  /**
   * @param {!ManagedProperties|undefined} newValue
   * @param {!ManagedProperties|undefined} oldValue
   * @private
   */
  managedPropertiesChanged_(newValue, oldValue) {
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
  },

  /**
   * @return {boolean} True if any input elements are currently being edited.
   * @private
   */
  isInputEditInProgress_: function() {
    if (!this.editable) {
      return false;
    }
    const activeElement = this.shadowRoot.activeElement;
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
  },

  /**
   * @param {?ManagedProxyLocation|undefined} a
   * @param {?ManagedProxyLocation|undefined} b
   * @return {boolean}
   * @private
   */
  proxyMatches_(a, b) {
    return !!a && !!b && a.host.activeValue === b.host.activeValue &&
        a.port.activeValue === b.port.activeValue;
  },

  /**
   * @param {number} port
   * @return {!ManagedProxyLocation}
   * @private
   */
  createDefaultProxyLocation_(port) {
    return {
      host: OncMojo.createManagedString(''),
      port: OncMojo.createManagedInt(port),
    };
  },

  /**
   * Returns a copy of |inputProxy| with all required properties set correctly.
   * @param {!ManagedProxySettings} inputProxy
   * @return {!ManagedProxySettings}
   * @private
   */
  validateProxy_(inputProxy) {
    const proxy =
        /** @type {!ManagedProxySettings} */ (Object.assign({}, inputProxy));
    const type = proxy.type.activeValue;
    if (type === 'PAC') {
      if (!proxy.pac) {
        proxy.pac = OncMojo.createManagedString('');
      }
    } else if (type === 'Manual') {
      proxy.manual = proxy.manual || this.savedManual_ || {};
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
          };
    }
    return proxy;
  },

  /** @private */
  updateProxy_() {
    if (!this.managedProperties) {
      return;
    }

    let proxySettings = this.managedProperties.proxySettings;

    // For shared networks with unmanaged proxy settings, ignore any saved proxy
    // settings and use the default value.
    if (this.isShared_() && proxySettings &&
        !this.isControlled(proxySettings.type) && !this.useSharedProxies) {
      proxySettings = null;  // Ignore proxy settings.
    }

    const proxy = proxySettings ? this.validateProxy_(proxySettings) :
                                  this.createDefaultProxySettings_();

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
    this.async(() => this.setProxy_(proxy));
  },

  /**
   * @param {!ManagedProxySettings} proxy
   * @private
   */
  setProxy_(proxy) {
    this.proxy_ = proxy;
    if (proxy.manual) {
      const manual = proxy.manual;
      const httpProxy = manual.httpProxy;
      if (this.proxyMatches_(httpProxy, manual.secureHttpProxy) &&
          this.proxyMatches_(httpProxy, manual.socks)) {
        // If all four proxies match, enable the 'use same proxy' toggle.
        this.useSameProxy_ = true;
      } else if (
          !manual.secureHttpProxy.host.activeValue &&
          !manual.socks.host.activeValue) {
        // Otherwise if no proxies other than http have a host value, also
        // enable the 'use same proxy' toggle.
        this.useSameProxy_ = true;
      }
    }
    this.proxyIsUserModified_ = false;
  },

  /** @private */
  useSameProxyChanged_() {
    this.proxyIsUserModified_ = true;
  },

  /**
   * @return {!ManagedProxySettings}
   * @private
   */
  createDefaultProxySettings_() {
    return {
      type: OncMojo.createManagedString('Direct'),
    };
  },

  /**
   * @param {?ManagedProxyLocation|undefined}
   *     location
   * @return {!ProxyLocation|undefined}
   * @private
   */
  getProxyLocation_(location) {
    if (!location) {
      return undefined;
    }
    return {
      host: location.host.activeValue,
      port: location.port.activeValue,
    };
  },

  /**
   * Called when the proxy changes in the UI.
   * @private
   */
  sendProxyChange_() {
    const proxyType = OncMojo.getActiveString(this.proxy_.type);
    if (!proxyType || (proxyType === 'PAC' && !this.proxy_.pac)) {
      return;
    }

    const proxy = /** @type {!ProxySettings} */ ({
      type: proxyType,
      excludeDomains: OncMojo.getActiveValue(this.proxy_.excludeDomains),
    });

    if (proxyType === 'Manual') {
      let manual = {};
      if (this.proxy_.manual) {
        this.savedManual_ =
            /** @type{!ManagedManualProxySettings}*/ (
                Object.assign({}, this.proxy_.manual));
        manual = {
          httpProxy: this.getProxyLocation_(this.proxy_.manual.httpProxy),
          secureHttpProxy:
              this.getProxyLocation_(this.proxy_.manual.secureHttpProxy),
          socks: this.getProxyLocation_(this.proxy_.manual.socks),
        };
      }
      if (this.proxy_.excludeDomains) {
        this.savedExcludeDomains_ =
            /** @type{!ManagedStringList}*/ (
                Object.assign({}, this.proxy_.excludeDomains));
      }
      const defaultProxy = manual.httpProxy || {host: '', port: 80};
      if (this.useSameProxy_) {
        manual.secureHttpProxy =
            /** @type {!ProxyLocation} */ (Object.assign({}, defaultProxy));
        manual.socks =
            /** @type {!ProxyLocation} */ (Object.assign({}, defaultProxy));
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
    this.fire('proxy-change', proxy);
    this.proxyIsUserModified_ = false;
  },

  /**
   * Event triggered when the selected proxy type changes.
   * @param {!Event} event
   * @private
   */
  onTypeChange_(event) {
    if (!this.proxy_ || !this.proxy_.type) {
      return;
    }
    const target = /** @type {!HTMLSelectElement} */ (event.target);
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
        elementToFocus = this.$$('#pacInput');
        // If a PAC is already set, send the type change now, otherwise wait
        // until the user provides a PAC value.
        proxyTypeChangeIsReady = !!OncMojo.getActiveString(this.proxy_.pac);
        break;
      case 'Manual':
        // Manual proxy configuration includes multiple input fields, so wait
        // until the 'send' button is clicked.
        proxyTypeChangeIsReady = false;
        elementToFocus = this.$$('#manualProxy network-proxy-input');
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
      this.async(() => {
        elementToFocus.focus();
      });
    }
  },

  /** @private */
  onPACChange_() {
    this.sendProxyChange_();
  },

  /** @private */
  onProxyInputChange_() {
    this.proxyIsUserModified_ = true;
  },

  /** @private */
  onAddProxyExclusionTap_() {
    assert(this.proxyExclusionInputValue_);
    this.push(
        'proxy_.excludeDomains.activeValue', this.proxyExclusionInputValue_);
    // Clear input.
    this.proxyExclusionInputValue_ = '';
    this.proxyIsUserModified_ = true;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddProxyExclusionKeypress_(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.onAddProxyExclusionTap_();
  },

  /**
   * @param {string} proxyExclusionInputValue
   * @return {boolean}
   * @private
   */
  shouldProxyExclusionButtonBeDisabled_(proxyExclusionInputValue) {
    return !proxyExclusionInputValue;
  },

  /**
   * Event triggered when the proxy exclusion list changes.
   * @param {!Event} event The remove proxy exclusions change event.
   * @private
   */
  onProxyExclusionsChange_(event) {
    this.proxyIsUserModified_ = true;
  },

  /** @private */
  onSaveProxyTap_() {
    this.sendProxyChange_();
  },

  /**
   * @param {string} proxyType The proxy type.
   * @return {string} The description for |proxyType|.
   * @private
   */
  getProxyTypeDesc_(proxyType) {
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
  },

  /**
   * @param {string} propertyName
   * @return {boolean} Whether the named property setting is editable.
   * @private
   */
  isEditable_(propertyName) {
    if (!this.editable || (this.isShared_() && !this.useSharedProxies)) {
      return false;
    }
    const property = /** @type {!OncMojo.ManagedProperty|undefined} */ (
        this.get('proxySettings.' + propertyName, this.managedProperties));
    if (!property) {
      return true;  // Default to editable if property is not defined.
    }
    return this.isPropertyEditable_(property);
  },

  /**
   * @param {!OncMojo.ManagedProperty|undefined} property
   * @return {boolean} Whether |property| is editable.
   * @private
   */
  isPropertyEditable_(property) {
    return !!property && !this.isNetworkPolicyEnforced(property) &&
        !this.isExtensionControlled(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  isShared_() {
    if (!this.managedProperties) {
      return false;
    }
    const source = this.managedProperties.source;
    return source === OncSource.kDevice || source === OncSource.kDevicePolicy;
  },

  /**
   * @return {boolean}
   * @private
   */
  isSaveManualProxyEnabled_() {
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
  },

  /**
   * @param {string} property The property to test
   * @param {string} value The value to test against
   * @return {boolean} True if property === value
   * @private
   */
  matches_(property, value) {
    return property === value;
  },
});

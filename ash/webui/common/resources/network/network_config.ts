// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'network-config' provides configuration of authentication properties for new
 * and existing networks.
 */

import '//resources/ash/common/cr_elements/action_link.css.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './network_config_input.js';
import './network_config_select.js';
import './network_config_toggle.js';
import './network_password_input.js';
import './network_shared.css.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {ConfigProperties, CrosNetworkConfigInterface, EAPConfigProperties, GlobalPolicy, IPSecConfigProperties, L2TPConfigProperties, ManagedBoolean, ManagedEAPProperties, ManagedInt32, ManagedIPSecProperties, ManagedL2TPProperties, ManagedOpenVPNProperties, ManagedProperties, ManagedString, ManagedStringList, ManagedWireGuardProperties, NetworkCertificate, OpenVPNConfigProperties, SubjectAltName, WireGuardConfigProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {CertificateType, HiddenSsidMode, SecurityType, StartConnectResult, VpnType} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, IPConfigType, NetworkType, OncSource, PolicySource} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {getTemplate} from './network_config.html.js';
import {NetworkConfigInputElement} from './network_config_input.js';
import {NetworkListenerBehavior} from './network_listener_behavior.js';
import type {NetworkPasswordInputElement} from './network_password_input.js';
import {OncMojo} from './onc_mojo.js';

enum VPNConfigType {
  IKEV2 = 'IKEv2',
  L2TP_IPSEC = 'L2TP_IPsec',
  OPEN_VPN = 'OpenVPN',
  WIREGUARD = 'WireGuard',
}

/**
 * Authentication types for IPsec-based VPNs.
 */
enum IpsecAuthType {
  PSK = 'PSK',
  CERT = 'Cert',
  EAP = 'EAP',
}

/**
 * Method to configure WireGuard local key pair.
 */
enum WireGuardKeyConfigType {
  USE_CURRENT = 'UseCurrent',
  GENERATE_NEW = 'GenerateNew',
  USER_INPUT = 'UserInput',
}

const DEFAULT_HASH = 'default';
const DO_NOT_CHECK_HASH = 'do-not-check';
const NO_CERTS_HASH = 'no-certs';
const NO_USER_CERT_HASH = 'no-user-cert';

const DEFAULT_EAP_OUTER_PROTOCOL = 'PEAP';

const PLACEHOLDER_CREDENTIAL = '(credential)';

const MIN_PASSPHRASE_LENGTH = 5;

/**
 * A light-weight regular expression for testing an IPv4 address string. Note
 * that this is not a complete check and thus some invalid input can also be
 * accepted.
 */
const IPV4_ADDR_REGEX = /^([0-9]+\.){3}[0-9]+$/i;

/**
 * A light-weight regular expression for testing an IPv6 address string. Note
 * that this is not a complete check and thus some invalid input can also be
 * accepted.
 */
const IPV6_ADDR_REGEX = /^(\:?[0-9a-f]{0,4}){2,8}$/i;

/**
 * A light-weight regular expression for testing an IP CIDR string (e.g.,
 * 192.168.1.0/24). Both IPv4 and IPv6 are accepted. Note that this is not a
 * complete check and thus some invalid input can also be accepted.
 */
const IP_CIDR_REGEX = /^[0-9a-f\.\:]+\/[0-9]+?$/i;

interface EapProperties {
  Outer: boolean;
  Inner: boolean;
  ServerCA: boolean;
  EapServerCertMatch: boolean;
  UserCert: boolean;
  Identity: boolean;
  Password: boolean;
  AnonymousIdentity: boolean;
}

interface VpnProperties {
  IPsec: boolean;
  IPsecPSK: boolean;
  IPsecEAP: boolean;
  IKEv2: boolean;
  OpenVPN: boolean;
  WireGuard: boolean;
  ServerCA: boolean;
  UserCert: boolean;
}

const NetworkConfigElementBase =
    mixinBehaviors([NetworkListenerBehavior], I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface,
    };

export class NetworkConfigElement extends NetworkConfigElementBase {
  static get is() {
    return 'network-config' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      globalPolicy_: Object,

      /**
       * The GUID when an existing network is being configured. This will be
       * empty when configuring a new network.
       */
      guid: String,

      name: String,

      /** The type of network being configured as a string. */
      type: String,

      /**
       * The type of network being configured as an enum.
       */
      mojoType_: {
        type: Number,
        value: undefined,
      },

      /**
         True if the user configuring the network can toggle the shared state.
       */
      shareAllowEnable: Boolean,

      /** The default shared state. */
      shareDefault: Boolean,

      enableConnect: {
        type: Boolean,
        notify: true,
        value: false,
      },

      enableSave: {
        type: Boolean,
        notify: true,
        value: false,
      },

      /**
       * Whether pressing the "Enter" key within the password field should start
       * a connection attempt. If this field is false, pressing "Enter" saves
       * the current configuration but does not connect.
       */
      connectOnEnter: {
        type: Boolean,
        value: false,
      },

      /** Set to any error from the last configuration result. */
      error: {
        type: String,
        notify: true,
      },

      /**
       * The prefilled network configuration. This can be empty if nothing to
       * prefill or the configuration will be synced according to `this.guid`.
       */
      prefilledProperties: Object,

      managedProperties_: {
        type: Object,
        value: null,
      },

      /**
       * Managed EAP properties used for determination of managed EAP fields.
       */
      managedEapProperties_: {
        type: Object,
        value: null,
      },

      /**
       * Set once managedProperties_ have been sent; prevents multiple saves.
       */
      propertiesSent_: {
        type: Boolean,
        value: false,
      },

      /**
       * The configuration properties for the network.
       */
      configProperties_: {
        type: Object,
        value: undefined,
      },

      /**
       * Reference to the EAP properties for the current type or null if all EAP
       * properties should be hidden (e.g. WiFi networks with non EAP Security).
       * Note: even though this references an entry in configProperties_, we
       * need to send a separate notification when it changes for data binding
       * (e.g. by using 'set').
       */
      eapProperties_: {
        type: Object,
      },

      /**
       * The cached result of installed server CA certificates.
       */
      cachedServerCaCerts_: {
        type: Array,
        value: undefined,
      },

      /**
       * The cached result of installed user certificates.
       */
      cachedUserCerts_: {
        type: Array,
        value: undefined,
      },

      /**
       * Used to populate the 'Server CA certificate' dropdown.
       */
      serverCaCerts_: {
        type: Array,
        value() {
          return [];
        },
      },

      selectedServerCaHash_: {
        type: String,
        value: undefined,
      },

      /**
       * Used to populate the 'User certificate' dropdown.
       */
      userCerts_: {
        type: Array,
        value() {
          return [];
        },
      },

      selectedUserCertHash_: {
        type: String,
        value: undefined,
        observer: 'updateIsConfigured_',
      },

      /**
       * Whether all required properties have been set.
       */
      isConfigured_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether this network should be shared with other users of the device.
       */
      shareNetwork_: {
        type: Boolean,
        value: true,
      },

      /**
       * This is a ManagedBoolean that represents a device-policy-enforced false
       * value. It is used to present a policy-disabled toggle for "Share
       * network" when user-created networks are ephemeral. It is never mutated.
       */
      shareNetworkEphemeralDisabled_: {
        type: Object,
        value: {
          activeValue: false,
          policySource: PolicySource.kDevicePolicyEnforced,
          policyValue: false,
        },
      },

      /**
       * Whether the device should automatically connect to the network.
       */
      autoConnect_: {
        type: Boolean,
        observer: 'updateHiddenNetworkWarning_',
      },

      /**
       * Whether or not to show the hidden network warning.
       */
      hiddenNetworkWarning_: Boolean,

      /**
       * Security value, used for Ethernet and Wifi and to detect when Security
       * changes. NOTE: the <select> element might set this to a string, see
       * crbug.com/1046149.
       */
      securityType_: Object,

      /**
       * 'SaveCredentials' value used for VPN (OpenVPN, IPsec, and L2TP).
       */
      vpnSaveCredentials_: {
        type: Boolean,
        value: false,
      },

      /**
       * VPN Type from vpnTypeItems_.
       */
      vpnType_: {
        type: String,
        value: undefined,
        observer: 'updateVpnIPsecAuthTypeItems_',
      },

      /**
       * Ipsec auth type from ipsecAuthTypeItems_.
       */
      ipsecAuthType_: {
        type: String,
        value: IpsecAuthType.PSK,
      },

      wireguardKeyType_: String,

      ipAddressInput_: {
        type: String,
        observer: 'updateIsConfigured_',
      },

      nameServersInput_: String,

      /**
       * Dictionary of boolean values determining which EAP properties to show,
       * or null to hide all EAP settings.
       */
      showEap_: {
        type: Object,
      },

      /**
       * Dictionary of boolean values determining which VPN properties to show,
       * or null to hide all VPN settings.
       */
      showVpn_: {
        type: Object,
      },

      /**
       * Whether the WireGuard private key input box should be shown.
       */
      isWireGuardUserPrivateKeyInputActive_: {
        type: Boolean,
        computed: 'computeWireGuardKeyType_(wireguardKeyType_)',
      },

      /**
       * Array of values for the EAP Method (Outer) dropdown.
       * They will be presented in a dropdown in this order.
       */
      eapOuterItems_: {
        type: Array,
        readOnly: true,
        value: ['PEAP', 'EAP-TLS', 'EAP-TTLS', 'LEAP'],
      },

      /**
       * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
       * when the Outer type is PEAP.
       */
      eapInnerItemsPeap_: {
        type: Array,
        readOnly: true,
        value: () => {
          const values = ['Automatic', 'MD5', 'MSCHAPv2'];
          if (loadTimeData.getBoolean('eapGtcWifiAuthentication')) {
            values.push('GTC');
          }
          return values;
        },
      },

      /**
       * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
       * when the Outer type is EAP-TTLS.
       */
      eapInnerItemsTtls_: {
        type: Array,
        readOnly: true,
        value: ['Automatic', 'MD5', 'MSCHAP', 'MSCHAPv2', 'PAP', 'CHAP', 'GTC'],
      },

      /**
       * Array of values for the VPN Type dropdown.
       * Note: closure does not recognize Array<VPNConfigType> here.
       */
      vpnTypeItems_: {
        type: Array,
        value: [
          VPNConfigType.L2TP_IPSEC,
          VPNConfigType.OPEN_VPN,
        ],
      },

      /**
       * Array of values for the Authentication Type dropdown for IPsec-based
       * VPNs.
       */
      ipsecAuthTypeItems_: {
        type: Array,
        value: [],
      },

      /**
       * Array of values for the WireGuard key configuration method dropdown.
       * The actual value is set in initWireGuardKeyConfigType_() since the
       * value differs for new network and existing networks.
       */
      wireguardKeyTypeItems_: {
        type: Array,
        value: [],
      },

      /**
       * Whether the current network configuration allows only device-wide
       * certificates (e.g. shared EAP TLS networks).
       */
      deviceCertsOnly_: {
        type: Boolean,
        value: false,
      },

      configRequiresPassphrase_: {
        type: Boolean,
        computed: 'computeConfigRequiresPassphrase_(mojoType_, securityType_)',
      },

      serializedDomainSuffixMatch_: {
        type: String,
        value: '',
      },

      serializedSubjectAltNameMatch_: {
        type: String,
        value: '',
      },
    };
  }

  guid: string;
  name: string;
  type: string;
  shareAllowEnable: boolean;
  shareDefault: boolean;
  enableConnect: boolean;
  enableSave: boolean;
  connectOnEnter: boolean;
  error: string;
  prefilledProperties: ConfigProperties|null;
  private globalPolicy_: GlobalPolicy|undefined;
  private mojoType_: NetworkType|undefined;
  private managedProperties_: ManagedProperties|null;
  private managedEapProperties_: ManagedEAPProperties|null;
  private propertiesSent_: boolean;
  private configProperties_: ConfigProperties|undefined;
  private eapProperties_: EAPConfigProperties|null;
  private cachedServerCaCerts_: NetworkCertificate[]|undefined;
  private cachedUserCerts_: NetworkCertificate[]|undefined;
  private serverCaCerts_: NetworkCertificate[];
  private userCerts_: NetworkCertificate[];
  private selectedServerCaHash_: string|undefined;
  private selectedUserCertHash_: string|undefined;
  private isConfigured_: boolean;
  private shareNetwork_: boolean;
  private shareNetworkEphemeralDisabled_: ManagedBoolean;
  private autoConnect_: boolean;
  private hiddenNetworkWarning_: boolean;
  private securityType_: SecurityType|string|undefined;
  private vpnSaveCredentials_: boolean;
  private vpnType_: VPNConfigType|undefined;
  private ipsecAuthType_: IpsecAuthType;
  private wireguardKeyType_: WireGuardKeyConfigType|undefined;
  private ipAddressInput_: string|undefined;
  private nameServersInput_: string|undefined;
  private showEap_: EapProperties|null;
  private showVpn_: VpnProperties|null;
  private isWireGuardUserPrivateKeyInputActive_: boolean;
  private eapOuterItems_: string[];
  private eapInnerItemsPeap_: string[];
  private eapInnerItemsTtls_: string[];
  private vpnTypeItems_: string[];
  private ipsecAuthTypeItems_: string[];
  private wireguardKeyTypeItems_: string[];
  private deviceCertsOnly_: boolean;
  private configRequiresPassphrase_: boolean;
  private serializedDomainSuffixMatch_: string;
  private serializedSubjectAltNameMatch_: string;
  private networkConfig_: CrosNetworkConfigInterface;

  static get observers() {
    return [
      'setEnableConnect_(isConfigured_, propertiesSent_)',
      'setEnableSave_(isConfigured_, managedProperties_)',
      'setShareNetwork_(mojoType_, managedProperties_, securityType_,' +
          'shareDefault, shareAllowEnable)',
      'updateConfigProperties_(mojoType_, managedProperties_)',
      'updateSecurity_(configProperties_, securityType_)',
      'updateCertItems_(cachedServerCaCerts_, cachedUserCerts_, vpnType_, ' +
          'securityType_, eapProperties_.outer)',
      'updateEapOuter_(eapProperties_.outer)',
      'updateEapCerts_(eapProperties_.*, serverCaCerts_, userCerts_)',
      'updateShowEap_(configProperties_.*, eapProperties_.*, securityType_)',
      'updateVpnType_(configProperties_, vpnType_, ipsecAuthType_)',
      'updateVpnIPsecCerts_(vpnType_, ipsecAuthType_,' +
          'configProperties_.typeConfig.vpn.ipSec.*, serverCaCerts_,' +
          'userCerts_)',
      'updateOpenVPNCerts_(vpnType_,' +
          'configProperties_.typeConfig.vpn.openVpn.*,' +
          'serverCaCerts_, userCerts_)',
      // Multiple updateIsConfigured observers for different configurations.
      'updateIsConfigured_(configProperties_.*, securityType_)',
      'updateIsConfigured_(configProperties_, eapProperties_.*)',
      'updateIsConfigured_(configProperties_.typeConfig.wifi.*)',
      'updateIsConfigured_(configProperties_.typeConfig.vpn.*, vpnType_,' +
          'ipsecAuthType_)',

    ];
  }

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.networkConfig_.getGlobalPolicy().then(response => {
      this.globalPolicy_ = response.result;
    });

    this.addEventListener('enter', this.onEnterEvent_);
  }

  init() {
    this.networkConfig_.getSupportedVpnTypes().then(response => {
      this.updateVpnTypeItems_(response.vpnTypes);
    });
    this.initWireGuardKeyConfigType_();

    if (this.guid) {
      this.networkConfig_.getManagedProperties(this.guid).then(response => {
        this.getManagedPropertiesCallback_(response.result);
      });
    } else {
      const mojoType = OncMojo.getNetworkTypeFromString(this.type);
      const managedProperties =
          OncMojo.getDefaultManagedProperties(mojoType, this.guid, this.name);
      // Allow securityType_ to be set externally (e.g. in tests).
      if (mojoType === NetworkType.kWiFi && this.securityType_ !== undefined) {
        assert(managedProperties.typeProperties.wifi);
        this.securityType_ = this.getSecurityTypeAsNumber(this.securityType_);
        managedProperties.typeProperties.wifi.security = this.securityType_;
      }
      this.managedProperties_ = managedProperties;
      this.mojoType_ = mojoType;
      setTimeout(() => {
        this.focusFirstInput_();
      });
    }

    if (this.mojoType_ === NetworkType.kVPN ||
        (this.globalPolicy_ &&
         this.globalPolicy_.allowOnlyPolicyWifiNetworksToConnect)) {
      this.autoConnect_ = false;
    } else {
      this.autoConnect_ = true;
    }
    this.hiddenNetworkWarning_ = this.showHiddenNetworkWarning_();

    this.updateIsConfigured_();
  }

  save(): void {
    this.saveAndConnect_(false /* connect */);
  }

  connect(): void {
    this.saveAndConnect_(true /* connect */);
  }


  private focusPassphrase_(): void {
    const passphraseInput =
        this.shadowRoot!.querySelector<NetworkPasswordInputElement>(
            '#wifi-passphrase');
    if (passphraseInput) {
      passphraseInput.focus();
    }
  }

  /**
   * @param  connect If true, connect after save.
   */
  private saveAndConnect_(connect: boolean): void {
    if (!this.managedProperties_ || this.propertiesSent_) {
      return;
    }
    this.propertiesSent_ = true;
    this.error = '';
    if (this.eapProperties_) {
      const dsm = OncMojo.deserializeDomainSuffixMatch(
          this.serializedDomainSuffixMatch_);
      if (!dsm) {
        this.setError_('invalidDomainSuffixMatchEntry');
        this.propertiesSent_ = false;
        return;
      }
      this.eapProperties_.domainSuffixMatch = dsm;

      const sanm = OncMojo.deserializeSubjectAltNameMatch(
          this.serializedSubjectAltNameMatch_);
      if (!sanm) {
        this.setError_('invalidSubjectAlternativeNameMatchEntry');
        this.propertiesSent_ = false;
        return;
      }
      this.eapProperties_.subjectAltNameMatch = sanm;
      if (!this.eapConfigServerCaCertAllowed_()) {
        this.setError_('missingEapDefaultServerCaSubjectVerification');
        this.propertiesSent_ = false;
        return;
      }
    }
    const propertiesToSet = this.getPropertiesToSet_();
    if (this.managedProperties_.source === OncSource.kNone) {
      // Explicitly set the hidden SSID mode of new WiFi networks disabled to
      // avoid networks being unintentionally marked as hidden in some
      // situations, e.g., when the network SSID is misspelled or the network is
      // not within range.
      if (this.mojoType_ === NetworkType.kWiFi) {
        assert(propertiesToSet.typeConfig.wifi);
        propertiesToSet.typeConfig.wifi.hiddenSsid = HiddenSsidMode.kDisabled;
      }
      if (!this.autoConnect_) {
        // Note: Do not set autoConnect to true, the connection manager will do
        // that on a successful connection (unless set to false here).
        propertiesToSet.autoConnect = {value: false};
      }
      this.networkConfig_.configureNetwork(propertiesToSet, this.shareNetwork_)
          .then(response => {
            this.createNetworkCallback_(
                response.guid, response.errorMessage, connect);
          });
    } else {
      this.networkConfig_.setProperties(this.guid, propertiesToSet)
          .then(response => {
            this.setPropertiesCallback_(
                response.success, response.errorMessage, connect);
          });
    }
    this.dispatchEvent(new CustomEvent('properties-set'));
  }

  private focusFirstInput_(): void {
    flush();
    const e = this.shadowRoot!.querySelector<HTMLElement>(
        'network-config-input:not([readonly]),' +
        'network-password-input:not([disabled]),' +
        'network-config-select:not([disabled])');
    if (e) {
      e.focus();
    }
  }

  private onEnterEvent_(event: Event): void {
    // TODO(crbug.com/379893491): Replace with assert.
    if ((event.composedPath()[0] as HTMLElement).localName ===
            'network-config-input' ||
        (event.composedPath()[0] as HTMLElement).localName ===
            'network-password-input') {
      this.onEnterPressedInInput_();
      event.stopPropagation();
    }
  }

  private onEnterPressedInInput_(): void {
    if (!this.isConfigured_) {
      return;
    }

    if (this.connectOnEnter) {
      this.connect();
    } else {
      this.save();
    }
  }

  private close_(): void {
    this.guid = '';
    this.type = '';
    this.securityType_ = undefined;
    this.dispatchEvent(new CustomEvent('close'));
  }

  private hasGuid_(): boolean {
    return !!this.guid;
  }

  private isIkev2Supported_(): boolean {
    return this.vpnTypeItems_.includes(VPNConfigType.IKEV2);
  }

  private isWireGuardSupported_(): boolean {
    return this.vpnTypeItems_.includes(VPNConfigType.WIREGUARD);
  }

  private updateVpnTypeItems_(responseTypes: string[]): void {
    this.vpnTypeItems_ = [
      VPNConfigType.L2TP_IPSEC,
      VPNConfigType.OPEN_VPN,
    ];
    if (responseTypes.includes('ikev2')) {
      this.vpnTypeItems_.unshift(VPNConfigType.IKEV2);
    }
    if (responseTypes.includes('wireguard')) {
      this.vpnTypeItems_.push(VPNConfigType.WIREGUARD);
    }
  }

  private initWireGuardKeyConfigType_(): void {
    let items = [
      WireGuardKeyConfigType.GENERATE_NEW,
      WireGuardKeyConfigType.USER_INPUT,
    ];
    if (this.hasGuid_()) {
      items = [...items, WireGuardKeyConfigType.USE_CURRENT];
      this.wireguardKeyType_ = WireGuardKeyConfigType.USE_CURRENT;
    } else {
      this.wireguardKeyType_ = WireGuardKeyConfigType.GENERATE_NEW;
    }
    this.wireguardKeyTypeItems_ = items;
  }

  /** NetworkListenerBehavior override */
  onNetworkCertificatesChanged() {
    this.networkConfig_.getNetworkCertificates().then(response => {
      this.set('cachedServerCaCerts_', response.serverCas.slice());
      this.set('cachedUserCerts_', response.userCerts.slice());
    });
  }

  private getDefaultCert_(type: CertificateType, desc: string, hash: string):
      NetworkCertificate {
    return {
      type: type,
      hash: hash,
      issuedBy: desc,
      issuedTo: '',
      pemOrId: '',
      availableForNetworkAuth: false,
      hardwareBacked: false,
      // Default cert entries should always be shown, even in the login UI,
      // so treat thiem as device-wide.
      deviceWide: true,
    };
  }

  private getActiveBoolean_(property: ManagedBoolean|undefined|null): boolean {
    if (!property) {
      return false;
    }
    return property.activeValue;
  }

  private getActiveInt32_(property: ManagedInt32|undefined|null): number {
    if (!property) {
      return 0;
    }
    return property.activeValue;
  }

  private getActiveStringList_(property: ManagedStringList|undefined|
                               null): string[]|null {
    if (!property) {
      return null;
    }
    return property.activeValue;
  }

  private getManagedPropertiesCallback_(managedProperties: ManagedProperties|
                                        null) {
    if (!managedProperties) {
      // The network no longer exists; close the page.
      console.warn('Network no longer exists: ' + this.guid);
      this.close_();
      return;
    }

    this.managedProperties_ = managedProperties;
    this.managedEapProperties_ = this.getManagedEap_(managedProperties);
    this.mojoType_ = managedProperties.type;

    if (this.mojoType_ === NetworkType.kVPN) {
      let saveCredentials = false;
      const vpn = managedProperties.typeProperties.vpn;
      assert(vpn);
      if (vpn.type === VpnType.kOpenVPN) {
        assert(vpn.openVpn);
        saveCredentials = this.getActiveBoolean_(vpn.openVpn.saveCredentials);
      } else if (vpn.type === VpnType.kIKEv2) {
        assert(vpn.ipSec);
        saveCredentials = this.getActiveBoolean_(vpn.ipSec.saveCredentials);
      } else if (vpn.type === VpnType.kL2TPIPsec) {
        assert(vpn.ipSec);
        assert(vpn.l2tp);
        saveCredentials = this.getActiveBoolean_(vpn.ipSec.saveCredentials) ||
            this.getActiveBoolean_(vpn.l2tp.saveCredentials);
      } else if (vpn.type === VpnType.kWireGuard) {
        saveCredentials = true;
      }
      this.vpnSaveCredentials_ = saveCredentials;
    }

    this.setError_(managedProperties.errorState);
    this.updateCertError_();
    this.focusFirstInput_();
  }

  private getSecurityItems_(): SecurityType[] {
    if (this.mojoType_ === NetworkType.kWiFi) {
      return [
        SecurityType.kNone,
        SecurityType.kWepPsk,
        SecurityType.kWpaPsk,
        SecurityType.kWpaEap,
      ];
    }
    return [
      SecurityType.kNone,
      SecurityType.kWpaEap,
    ];
  }

  private setShareNetwork_(): void {
    if (this.mojoType_ === undefined || !this.managedProperties_ ||
        !this.securityType_ === undefined) {
      return;
    }
    const source = this.managedProperties_.source;
    if (source !== OncSource.kNone) {
      // Configured networks can not change whether they are shared.
      this.shareNetwork_ =
          source === OncSource.kDevice || source === OncSource.kDevicePolicy;
      return;
    }
    if (!this.shareIsVisible_()) {
      this.shareNetwork_ = this.shareDefault;
      return;
    }
    if (this.shareAllowEnable && !this.shareDefault) {
      // By default, Wi-Fi networks which require passwords are not shared,
      // but "insecure" networks with no passwords are shared. In either case,
      // the user can change the sharing setting by updating the toggle in the
      // UI.

      if (this.mojoType_ === NetworkType.kWiFi) {
        this.shareNetwork_ = this.securityType_ === SecurityType.kNone;
        return;
      }
    }
    this.shareNetwork_ = this.shareDefault;
  }

  private onShareChanged_(_event: Event): void {
    this.updateSelectedCerts_();
  }

  private getEAPConfigProperties_(eap: ManagedEAPProperties):
      EAPConfigProperties {
    return {
      anonymousIdentity: OncMojo.getActiveString(eap.anonymousIdentity),
      clientCertType: OncMojo.getActiveString(eap.clientCertType),
      clientCertPkcs11Id: OncMojo.getActiveString(eap.clientCertPkcs11Id),
      domainSuffixMatch: this.getActiveStringList_(eap.domainSuffixMatch) || [],
      identity: OncMojo.getActiveString(eap.identity),
      inner: OncMojo.getActiveString(eap.inner),
      outer: OncMojo.getActiveString(eap.outer) || DEFAULT_EAP_OUTER_PROTOCOL,
      password: OncMojo.getActiveString(eap.password),
      saveCredentials: this.getActiveBoolean_(eap.saveCredentials),
      serverCaPems: this.getActiveStringList_(eap.serverCaPems),
      subjectAltNameMatch: (OncMojo.getActiveValue(eap.subjectAltNameMatch) as
                            unknown) as SubjectAltName[] ||
          [],
      subjectMatch: OncMojo.getActiveString(eap.subjectMatch),
      useSystemCas: this.getActiveBoolean_(eap.useSystemCas),
    };
  }

  private getIPSecConfigProperties_(ipSec: ManagedIPSecProperties):
      IPSecConfigProperties {
    return {
      authenticationType:
          OncMojo.getActiveString(ipSec.authenticationType) || 'PSK',
      clientCertPkcs11Id: OncMojo.getActiveString(ipSec.clientCertPkcs11Id),
      clientCertType: OncMojo.getActiveString(ipSec.clientCertType),
      eap: ipSec.eap ? this.getEAPConfigProperties_(ipSec.eap) : null,
      group: OncMojo.getActiveString(ipSec.group),
      ikeVersion: this.getActiveInt32_(ipSec.ikeVersion),
      localIdentity: OncMojo.getActiveString(ipSec.localIdentity),
      psk: OncMojo.getActiveString(ipSec.psk),
      remoteIdentity: OncMojo.getActiveString(ipSec.remoteIdentity),
      saveCredentials: this.getActiveBoolean_(ipSec.saveCredentials),
      serverCaPems: this.getActiveStringList_(ipSec.serverCaPems),
      serverCaRefs: this.getActiveStringList_(ipSec.serverCaRefs),
    };
  }

  private getL2TPConfigProperties_(l2tp: ManagedL2TPProperties):
      L2TPConfigProperties {
    return {
      lcpEchoDisabled: this.getActiveBoolean_(l2tp.lcpEchoDisabled),
      password: OncMojo.getActiveString(l2tp.password),
      saveCredentials: this.getActiveBoolean_(l2tp.saveCredentials),
      username: OncMojo.getActiveString(l2tp.username),
    };
  }

  private getOpenVPNConfigProperties_(openVpn: ManagedOpenVPNProperties):
      OpenVPNConfigProperties {
    return {
      clientCertPkcs11Id: OncMojo.getActiveString(openVpn.clientCertPkcs11Id),
      clientCertType: OncMojo.getActiveString(openVpn.clientCertType),
      extraHosts: this.getActiveStringList_(openVpn.extraHosts),
      otp: '',
      password: OncMojo.getActiveString(openVpn.password),
      saveCredentials: this.getActiveBoolean_(openVpn.saveCredentials),
      serverCaPems: this.getActiveStringList_(openVpn.serverCaPems),
      serverCaRefs: this.getActiveStringList_(openVpn.serverCaRefs),
      userAuthenticationType:
          OncMojo.getActiveString(openVpn.userAuthenticationType),
      username: OncMojo.getActiveString(openVpn.username),
    };
  }

  private getWireGuardConfigProperties_(wireguard: ManagedWireGuardProperties):
      WireGuardConfigProperties {
    const config: WireGuardConfigProperties = {
      ipAddresses: this.getActiveStringList_(wireguard.ipAddresses) ?? [],
      privateKey: OncMojo.getActiveString(wireguard.privateKey),
      peers: [],
    };
    if (wireguard.peers && wireguard.peers.activeValue) {
      for (const peer of wireguard.peers.activeValue) {
        const peerCopied = Object.assign({}, peer);
        if (this.hasGuid_()) {
          // Shill does not return exact value for credential fields, showing
          // a placeholder here.
          peerCopied.presharedKey = PLACEHOLDER_CREDENTIAL;
        }
        assert(config.peers);
        config.peers.push(peerCopied);
      }
    }
    return config;
  }

  /**
   * Updates the config properties when |this.managedProperties| changes.
   * This gets called once when navigating to the page when default properties
   * are set, and again for existing networks when the properties are received.
   */
  private updateConfigProperties_(): void {
    if (this.mojoType_ === undefined || !this.managedProperties_) {
      return;
    }
    this.showEap_ = null;
    this.showVpn_ = null;
    this.vpnType_ = undefined;

    const managedProperties = this.managedProperties_;
    const configProperties =
        OncMojo.getBaselineConfigProperties(managedProperties);
    configProperties.name = OncMojo.getActiveString(managedProperties.name);

    let autoConnect;
    let security = SecurityType.kNone;
    switch (managedProperties.type) {
      case NetworkType.kWiFi:
        const wifi = managedProperties.typeProperties.wifi;
        assert(wifi);
        autoConnect = this.getActiveBoolean_(wifi.autoConnect);
        const configWifi = configProperties.typeConfig.wifi;
        assert(configWifi);
        configWifi.passphrase = OncMojo.getActiveString(wifi.passphrase);
        configWifi.ssid = OncMojo.getActiveString(wifi.ssid);
        if (wifi.eap) {
          configWifi.eap = this.getEAPConfigProperties_(wifi.eap);
        }
        // updateSecurity_ will ensure that EAP properties are set correctly.
        security = wifi.security;
        configWifi.security = security;
        break;
      case NetworkType.kEthernet:
        assert(managedProperties.typeProperties.ethernet);
        const eap = managedProperties.typeProperties.ethernet.eap ?
            this.getEAPConfigProperties_(
                managedProperties.typeProperties.ethernet.eap) :
            null;
        security = eap ? SecurityType.kWpaEap : SecurityType.kNone;
        const auth = security === SecurityType.kWpaEap ? '8021X' : 'None';
        assert(configProperties.typeConfig.ethernet);
        configProperties.typeConfig.ethernet.authentication = auth;
        configProperties.typeConfig.ethernet.eap = eap;
        break;
      case NetworkType.kVPN:
        const vpn = managedProperties.typeProperties.vpn;
        assert(vpn);
        const vpnType = vpn.type;
        const configVpn = configProperties.typeConfig.vpn;
        assert(configVpn);
        configVpn.host = OncMojo.getActiveString(vpn.host);
        configVpn.type = {value: vpnType};
        if (vpnType === VpnType.kIKEv2) {
          if (!this.isIkev2Supported_()) {
            break;
          }
          assert(vpn.ipSec);
          configVpn.ipSec = this.getIPSecConfigProperties_(vpn.ipSec);
        } else if (vpnType === VpnType.kL2TPIPsec) {
          assert(vpn.ipSec);
          configVpn.ipSec = this.getIPSecConfigProperties_(vpn.ipSec);
          assert(vpn.l2tp);
          configVpn.l2tp = this.getL2TPConfigProperties_(vpn.l2tp);
        } else if (vpnType === VpnType.kOpenVPN) {
          assert(vpn.openVpn);
          configVpn.openVpn = this.getOpenVPNConfigProperties_(vpn.openVpn);
        } else if (vpnType === VpnType.kWireGuard) {
          if (!this.isWireGuardSupported_()) {
            break;
          }
          assert(vpn.wireguard);
          configVpn.wireguard =
              this.getWireGuardConfigProperties_(vpn.wireguard);
          assert(configVpn.wireguard.ipAddresses);
          this.ipAddressInput_ = configVpn.wireguard.ipAddresses.join(',');
          const staticIpConfig = managedProperties.staticIpConfig;
          if (staticIpConfig && staticIpConfig.nameServers) {
            this.nameServersInput_ =
                staticIpConfig.nameServers.activeValue.join(',');
          }
        } else {
          assertNotReached();
        }
        security = SecurityType.kNone;
        break;
    }
    if (autoConnect !== undefined) {
      configProperties.autoConnect = {value: autoConnect};
    }
    // Request certificates the first time |configProperties_| is set.
    const requestCertificates = this.configProperties_ === undefined;
    this.configProperties_ = configProperties;
    this.securityType_ = security;
    assert(this.configProperties_);
    this.set('eapProperties_', this.getEap_(this.configProperties_));
    if (!this.eapProperties_) {
      this.showEap_ = null;
    } else {
      this.serializedDomainSuffixMatch_ = OncMojo.serializeDomainSuffixMatch(
          this.eapProperties_.domainSuffixMatch);
      this.serializedSubjectAltNameMatch_ =
          OncMojo.serializeSubjectAltNameMatch(
              this.eapProperties_.subjectAltNameMatch);
    }
    if (managedProperties.type === NetworkType.kVPN) {
      this.vpnType_ = this.getVpnTypeFromProperties_(this.configProperties_);
      this.ipsecAuthType_ =
          this.getIpsecAuthTypeFromProperties_(this.configProperties_);
    }
    if (requestCertificates) {
      this.onNetworkCertificatesChanged();
    }
  }

  /**
   * Ensures that the appropriate properties are set or deleted when
   * |securityType_| changes.
   */
  private updateSecurity_(): void {
    if (this.securityType_ === undefined || !this.configProperties_) {
      return;
    }
    const type = this.mojoType_;
    this.securityType_ = this.getSecurityTypeAsNumber(this.securityType_);
    const security = this.securityType_;
    if (type === NetworkType.kWiFi) {
      assert(this.configProperties_.typeConfig.wifi);
      this.configProperties_.typeConfig.wifi.security = security;
    } else if (type === NetworkType.kEthernet) {
      const auth = security === SecurityType.kWpaEap ? '8021X' : 'None';
      assert(this.configProperties_.typeConfig.ethernet);
      this.configProperties_.typeConfig.ethernet.authentication = auth;
    }
    let eap = null;
    if (security === SecurityType.kWpaEap) {
      eap = this.getEap_(this.configProperties_, true);
      assert(eap);
      eap.outer = eap.outer || DEFAULT_EAP_OUTER_PROTOCOL;
    }
    this.setEap_(eap);
  }

  /**
   * Ensures that the appropriate EAP properties are created (or deleted when
   * the eap.outer property changes.
   */
  private updateEapOuter_(): void {
    const eap = this.eapProperties_;
    if (!eap || !eap.outer) {
      return;
    }
    const innerItems = this.getEapInnerItems_(eap.outer);
    if (innerItems.length > 0) {
      if (!eap.inner || innerItems.indexOf(eap.inner) < 0) {
        this.set('eapProperties_.inner', innerItems[0]);
      }
    } else {
      this.set('eapProperties_.inner', undefined);
    }

    if (eap.outer !== 'EAP-TLS') {
      this.set('eapProperties_.clientCertType', 'None');
      this.set('eapProperties_.clientCertPkcs11Id', '');
      this.selectedUserCertHash_ = NO_USER_CERT_HASH;
    }
  }

  private updateEapCerts_(): void {
    // EAP is used for all configurable types except VPN.
    if (this.mojoType_ === NetworkType.kVPN) {
      return;
    }
    const eap = this.eapProperties_;
    const pem = eap && eap.serverCaPems ? eap.serverCaPems[0] : '';
    const certId =
        eap && eap.clientCertType === 'PKCS11Id' ? eap.clientCertPkcs11Id : '';
    this.setSelectedCerts_(pem, certId);
  }

  private updateShowEap_(): void {
    if (!this.eapProperties_ || this.securityType_ === SecurityType.kNone) {
      this.showEap_ = null;
      this.updateCertError_();
      return;
    }
    const outer = this.eapProperties_.outer;
    switch (this.mojoType_) {
      case NetworkType.kWiFi:
      case NetworkType.kEthernet:
        this.showEap_ = {
          Outer: true,
          Inner: outer === 'PEAP' || outer === 'EAP-TTLS',
          ServerCA: outer !== 'LEAP',
          EapServerCertMatch:
              outer === 'EAP-TLS' || outer === 'EAP-TTLS' || outer === 'PEAP',
          UserCert: outer === 'EAP-TLS',
          Identity: true,
          Password: outer !== 'EAP-TLS',
          AnonymousIdentity: outer === 'PEAP' || outer === 'EAP-TTLS',
        };
        break;
    }
    this.updateCertError_();
  }

  private getEap_(
      properties: ConfigProperties,
      optCreate: boolean|undefined = undefined): EAPConfigProperties|null {
    let eap;
    if (properties.typeConfig.wifi) {
      eap = properties.typeConfig.wifi.eap;
    } else if (properties.typeConfig.ethernet) {
      eap = properties.typeConfig.ethernet.eap;
    } else if (properties.typeConfig.vpn && properties.typeConfig.vpn.ipSec) {
      eap = properties.typeConfig.vpn.ipSec.eap;
    }
    if (optCreate) {
      return eap || {
        saveCredentials: false,
        useSystemCas: false,
        domainSuffixMatch: [],
        subjectAltNameMatch: [],
        anonymousIdentity: null,
        clientCertPkcs11Id: null,
        clientCertType: null,
        identity: null,
        inner: null,
        outer: null,
        password: null,
        serverCaPems: null,
        subjectMatch: null,
      };
    }
    return eap || null;
  }

  private setEap_(eapProperties: EAPConfigProperties|null) {
    assert(this.configProperties_);
    switch (this.mojoType_) {
      case NetworkType.kWiFi:
        assert(this.configProperties_.typeConfig.wifi);
        this.configProperties_.typeConfig.wifi.eap = eapProperties;
        break;
      case NetworkType.kEthernet: {
        assert(this.configProperties_.typeConfig.ethernet);
        this.configProperties_.typeConfig.ethernet.eap = eapProperties;
        break;
      }
    }
    this.set('eapProperties_', eapProperties);
  }

  private getManagedEap_(managedProperties: ManagedProperties):
      ManagedEAPProperties|null {
    let managedEap;
    switch (managedProperties.type) {
      case NetworkType.kWiFi: {
        assert(managedProperties.typeProperties.wifi);
        managedEap = managedProperties.typeProperties.wifi.eap;
        break;
      }
      case NetworkType.kEthernet: {
        assert(managedProperties.typeProperties.ethernet);
        managedEap = managedProperties.typeProperties.ethernet.eap;
        break;
      }
      case NetworkType.kVPN: {
        assert(managedProperties.typeProperties.vpn);
        if (managedProperties.typeProperties.vpn.ipSec) {
          managedEap = managedProperties.typeProperties.vpn.ipSec.eap;
        }
        break;
      }
    }
    return managedEap || null;
  }

  private getVpnTypeFromProperties_(properties: ConfigProperties):
      VPNConfigType {
    const vpn = properties.typeConfig.vpn;
    assert(vpn);
    if (!!vpn.type && vpn.type.value === VpnType.kIKEv2) {
      return VPNConfigType.IKEV2;
    } else if (!!vpn.type && vpn.type.value === VpnType.kL2TPIPsec) {
      return VPNConfigType.L2TP_IPSEC;
    } else if (!!vpn.type && vpn.type.value === VpnType.kWireGuard) {
      return VPNConfigType.WIREGUARD;
    }
    return VPNConfigType.OPEN_VPN;
  }

  private getIpsecAuthTypeFromProperties_(properties: ConfigProperties):
      IpsecAuthType {
    const vpn = properties.typeConfig.vpn;
    assert(vpn);
    if (!vpn.type ||
        !(vpn.type.value === VpnType.kL2TPIPsec ||
          vpn.type.value === VpnType.kIKEv2)) {
      // This field will not be used by services other than IPsec-based VPN.
      // Initiate it to "PSK" for simplicity.
      return IpsecAuthType.PSK;
    }
    assert(vpn.ipSec);
    if (vpn.ipSec.authenticationType === IpsecAuthType.PSK) {
      return IpsecAuthType.PSK;
    } else if (vpn.ipSec.authenticationType === IpsecAuthType.CERT) {
      return IpsecAuthType.CERT;
    } else if (vpn.ipSec.authenticationType === IpsecAuthType.EAP) {
      return IpsecAuthType.EAP;
    }
    assertNotReached();
  }

  private computeWireGuardKeyType_(): boolean {
    return this.wireguardKeyType_ === WireGuardKeyConfigType.USER_INPUT;
  }

  private updateCertItems_(): void {
    if (this.configProperties_ === undefined ||
        this.cachedServerCaCerts_ === undefined ||
        this.cachedUserCerts_ === undefined) {
      return;
    }

    const isOpenVpn = this.vpnType_ === VPNConfigType.OPEN_VPN;
    const isIpsec = this.vpnType_ === VPNConfigType.L2TP_IPSEC ||
        this.vpnType_ === VPNConfigType.IKEV2;
    let caCerts = this.cachedServerCaCerts_.slice();
    if (!isOpenVpn && !isIpsec) {
      // 'Default' is the same as 'Do not check' except that 'Default' sets
      // eap.useSystemCas (which does not apply to OpenVPN and IPsec-based
      // VPNs).
      caCerts.unshift(this.getDefaultCert_(
          CertificateType.kServerCA, this.i18n('networkCAUseDefault'),
          DEFAULT_HASH));
    }
    if (!isIpsec) {
      // For IPsec-based VPNs, it is mandatory to verify the server.
      caCerts.push(this.getDefaultCert_(
          CertificateType.kServerCA, this.i18n('networkCADoNotCheck'),
          DO_NOT_CHECK_HASH));
    }
    if (!caCerts.length) {
      caCerts = [this.getDefaultCert_(
          CertificateType.kServerCA,
          this.i18n('networkCertificateNoneInstalled'), NO_CERTS_HASH)];
    }
    this.set('serverCaCerts_', caCerts);

    let userCerts = this.cachedUserCerts_.slice();
    // Only certs available for network authentication can be used.
    userCerts.forEach(function(cert) {
      if (!cert.availableForNetworkAuth) {
        cert.hash = '';
      }  // Clear the hash to invalidate the certificate.
    });

    const isEap = this.securityType_ === SecurityType.kWpaEap;
    const isEapTls =
        isEap && this.eapProperties_ && this.eapProperties_.outer === 'EAP-TLS';

    // User certificate is allowed but not required for OpenVPN and
    // EAP protocols except EAP-TLS (required for EAP-TLS)
    const isUserCertOptional = isOpenVpn || (isEap && !isEapTls);

    if (isUserCertOptional) {
      userCerts.unshift(this.getDefaultCert_(
          CertificateType.kUserCert, this.i18n('networkNoUserCert'),
          NO_USER_CERT_HASH));
    }
    if (!userCerts.length) {
      userCerts = [this.getDefaultCert_(
          CertificateType.kUserCert,
          this.i18n('networkCertificateNoneInstalled'), NO_CERTS_HASH)];
    }
    this.set('userCerts_', userCerts);

    this.updateSelectedCerts_();
    this.updateCertError_();
  }

  private updateVpnType_(): void {
    if (this.configProperties_ === undefined || this.vpnType_ === undefined) {
      return;
    }

    const vpn = this.configProperties_.typeConfig.vpn;
    if (!vpn) {
      this.showVpn_ = null;
      this.updateCertError_();
      return;
    }
    switch (this.vpnType_) {
      case VPNConfigType.IKEV2:
        vpn.type = {value: VpnType.kIKEv2};
        if (!vpn.ipSec) {
          this.ipsecAuthType_ = IpsecAuthType.EAP;
          vpn.ipSec = {
            authenticationType: this.ipsecAuthType_,
            ikeVersion: 2,
            saveCredentials: false,
            clientCertPkcs11Id: null,
            clientCertType: null,
            eap: null,
            group: null,
            localIdentity: null,
            psk: null,
            remoteIdentity: null,
            serverCaPems: null,
            serverCaRefs: null,
          };
        }
        assert(vpn.ipSec);
        if (this.ipsecAuthType_ === IpsecAuthType.EAP && !vpn.ipSec.eap) {
          vpn.ipSec.eap = {
            domainSuffixMatch: [],
            outer: 'MSCHAPv2',
            saveCredentials: false,
            subjectAltNameMatch: [],
            useSystemCas: false,
            anonymousIdentity: null,
            clientCertPkcs11Id: null,
            clientCertType: null,
            identity: null,
            inner: null,
            password: null,
            serverCaPems: null,
            subjectMatch: null,
          };
          assert(vpn.ipSec.eap);
          this.eapProperties_ = vpn.ipSec.eap;
        }
        break;
      case VPNConfigType.L2TP_IPSEC:
        vpn.type = {value: VpnType.kL2TPIPsec};
        if (this.ipsecAuthType_ !== IpsecAuthType.PSK &&
            this.ipsecAuthType_ !== IpsecAuthType.CERT) {
          // This will happen if user changes the VPN type to IKEv2 where the
          // default value of auth type is EAP, and then changes the VPN type to
          // L2TP/IPsec.
          this.ipsecAuthType_ = IpsecAuthType.PSK;
        }

        if (!vpn.ipSec) {
          vpn.ipSec = {
            authenticationType: this.ipsecAuthType_,
            ikeVersion: 1,
            saveCredentials: false,
            clientCertPkcs11Id: null,
            clientCertType: null,
            eap: null,
            group: null,
            localIdentity: null,
            psk: null,
            remoteIdentity: null,
            serverCaPems: null,
            serverCaRefs: null,
          };
        }
        break;
      case VPNConfigType.OPEN_VPN:
        vpn.type = {value: VpnType.kOpenVPN};
        vpn.openVpn = vpn.openVpn || {
          saveCredentials: false,
          clientCertPkcs11Id: null,
          clientCertType: null,
          extraHosts: null,
          otp: null,
          password: null,
          serverCaPems: null,
          serverCaRefs: null,
          username: null,
          userAuthenticationType: null,
        };
        break;
      case VPNConfigType.WIREGUARD:
        vpn.type = {value: VpnType.kWireGuard};
        vpn.wireguard = vpn.wireguard || {
          peers: [{
            publicKey: '',
            presharedKey: null,
            allowedIps: null,
            endpoint: null,
            persistentKeepaliveInterval: 0,
          }],
          ipAddresses: null,
          privateKey: null,
        };
        break;
      default:
        assertNotReached();
    }

    const isIpsec = this.vpnType_ === VPNConfigType.L2TP_IPSEC ||
        this.vpnType_ === VPNConfigType.IKEV2;
    const ipsecAuthIsPsk = this.ipsecAuthType_ === IpsecAuthType.PSK;
    const ipsecAuthIsEap = this.ipsecAuthType_ === IpsecAuthType.EAP;
    const ipsecAuthIsCert = this.ipsecAuthType_ === IpsecAuthType.CERT;
    const isOpenvpn = this.vpnType_ === VPNConfigType.OPEN_VPN;
    this.showVpn_ = {
      IPsec: isIpsec,
      IPsecPSK: isIpsec && ipsecAuthIsPsk,
      IPsecEAP: isIpsec && ipsecAuthIsEap,
      IKEv2: this.vpnType_ === VPNConfigType.IKEV2,
      OpenVPN: isOpenvpn,
      WireGuard: this.vpnType_ === VPNConfigType.WIREGUARD,
      ServerCA: (isIpsec && !ipsecAuthIsPsk) || isOpenvpn,
      UserCert: (isIpsec && ipsecAuthIsCert) || isOpenvpn,
    };

    if (vpn.type.value === VpnType.kL2TPIPsec && !vpn.l2tp) {
      vpn.l2tp = {
        lcpEchoDisabled: false,
        password: '',
        saveCredentials: false,
        username: '',
      };
    }
    if (vpn.type.value !== VpnType.kL2TPIPsec &&
        vpn.type.value !== VpnType.kIKEv2) {
      vpn.ipSec = null;
    }
    if (vpn.type.value !== VpnType.kL2TPIPsec) {
      vpn.l2tp = null;
    }
    if (vpn.type.value !== VpnType.kOpenVPN) {
      vpn.openVpn = null;
    }
    if (vpn.type.value !== VpnType.kWireGuard) {
      vpn.wireguard = null;
    }
    this.updateCertError_();
  }

  private updateVpnIPsecAuthTypeItems_(): void {
    this.ipsecAuthTypeItems_ = [
      IpsecAuthType.PSK,
      IpsecAuthType.CERT,
    ];
    if (this.vpnType_ === VPNConfigType.IKEV2) {
      this.ipsecAuthTypeItems_.push(IpsecAuthType.EAP);
    }
  }

  private updateVpnIPsecCerts_(): void {
    if (this.vpnType_ !== VPNConfigType.L2TP_IPSEC &&
        this.vpnType_ !== VPNConfigType.IKEV2) {
      return;
    }
    if (this.ipsecAuthType_ === IpsecAuthType.PSK) {
      return;
    }
    assert(this.configProperties_?.typeConfig.vpn);
    const ipSec = this.configProperties_.typeConfig.vpn.ipSec;
    if (!ipSec) {
      return;
    }
    const pem = ipSec.serverCaPems ? ipSec.serverCaPems[0] : null;
    const certId =
        ipSec.clientCertType === 'PKCS11Id' ? ipSec.clientCertPkcs11Id : '';
    this.setSelectedCerts_(pem, certId);
  }

  private updateOpenVPNCerts_(): void {
    if (this.vpnType_ !== VPNConfigType.OPEN_VPN) {
      return;
    }
    assert(this.configProperties_?.typeConfig.vpn);
    const openVpn = this.configProperties_.typeConfig.vpn.openVpn;
    if (!openVpn) {
      return;
    }
    const pem = openVpn.serverCaPems ? openVpn.serverCaPems[0] : null;
    const certId =
        openVpn.clientCertType === 'PKCS11Id' ? openVpn.clientCertPkcs11Id : '';
    this.setSelectedCerts_(pem, certId);
  }

  private updateCertError_(): void {
    // If |this.error| was set to something other than a cert error, do not
    // change it.
    const noCertsError = 'networkErrorNoUserCertificate';
    const noValidCertsError = 'networkErrorNotAvailableForNetworkAuth';
    if (this.error && this.error !== noCertsError &&
        this.error !== noValidCertsError) {
      return;
    }

    const requireCerts = (this.showEap_ && this.showEap_.UserCert) ||
        (this.showVpn_ && this.showVpn_.UserCert);
    if (!requireCerts) {
      this.setError_('');
      return;
    }
    if (!this.userCerts_.length || this.userCerts_[0].hash === NO_CERTS_HASH) {
      this.setError_(noCertsError);
      return;
    }
    const validUserCert = this.userCerts_.find(function(cert) {
      return !!cert.hash;
    });
    if (!validUserCert) {
      this.setError_(noValidCertsError);
      return;
    }
    this.setError_('');
    return;
  }

  /**
   * Sets the selected cert if |pem| (serverCa) or |certId| (user) is specified.
   * Otherwise sets a default value if no certificate is selected.
   */
  private setSelectedCerts_(pem: string|null, certId: string|null): void {
    if (pem) {
      const serverCa = this.serverCaCerts_.find(function(cert) {
        return cert.pemOrId === pem;
      });
      if (serverCa) {
        this.selectedServerCaHash_ = serverCa.hash;
      }
    }

    if (certId) {
      // |certId| is in the format |slot:id| for EAP and IPSec and |id| for
      // OpenVPN certs.
      // |userCerts_[i].pemOrId| is always in the format |slot:id|.
      // Use a substring comparison to support both |certId| formats.
      const userCert = this.userCerts_.find(function(cert) {
        return cert.pemOrId.indexOf(certId) >= 0;
      });
      if (userCert) {
        this.selectedUserCertHash_ = userCert.hash;
      }
    }
    this.updateSelectedCerts_();
    this.updateIsConfigured_();
  }

  private findCert_(certs: NetworkCertificate[], hash: string|undefined):
      NetworkCertificate|undefined {
    if (!hash) {
      return undefined;
    }

    return certs.find((cert) => {
      return cert.hash === hash;
    });
  }

  /**
   * Called when the certificate list or a selected certificate changes.
   * Ensures that each selected certificate exists in its list, or selects the
   * correct default value.
   */
  private updateSelectedCerts_(): void {
    if (!this.serverCaCerts_.length || !this.userCerts_.length) {
      return;
    }
    const eap = this.eapProperties_;

    // Only device-wide certificates can be used for shared networks that
    // require a certificate.
    this.deviceCertsOnly_ =
        this.shareNetwork_ && !!eap && eap.outer === 'EAP-TLS';

    // Validate selected Server CA.
    const caCert =
        this.findCert_(this.serverCaCerts_, this.selectedServerCaHash_);
    if (!caCert || (this.deviceCertsOnly_ && !caCert.deviceWide)) {
      this.selectedServerCaHash_ = undefined;
    }
    if (!this.selectedServerCaHash_) {
      if (eap && eap.useSystemCas) {
        this.selectedServerCaHash_ = DEFAULT_HASH;
      } else if (!this.guid && this.serverCaCerts_[0]) {
        // For unconfigured networks, default to the first available
        // certificate and fallback to DEFAULT_HASH. See
        // onNetworkCertificatesChanged() for how certificates are added.
        let cert = this.serverCaCerts_[0];
        if (cert.hash === DEFAULT_HASH &&
            this.isRealCertUsableForNetworkAuth_(this.serverCaCerts_[1])) {
          cert = this.serverCaCerts_[1];
        }
        this.selectedServerCaHash_ = cert.hash;
      } else {
        this.selectedServerCaHash_ = DO_NOT_CHECK_HASH;
      }
    }

    // Validate selected User cert.
    const userCert =
        this.findCert_(this.userCerts_, this.selectedUserCertHash_);
    if (!userCert || (this.deviceCertsOnly_ && !userCert.deviceWide)) {
      this.selectedUserCertHash_ = undefined;
    }
    if (!this.selectedUserCertHash_) {
      for (let i = 0; i < this.userCerts_.length; ++i) {
        const userCert = this.userCerts_[i];
        if (userCert && (!this.deviceCertsOnly_ || userCert.deviceWide)) {
          this.selectedUserCertHash_ = userCert.hash;
          break;
        }
      }
    }
  }
  /**
   * Checks that the hash of the certificate is set and not one of the default
   * special strings.
   */
  private isRealCertUsableForNetworkAuth_(cert: NetworkCertificate|
                                          undefined): boolean {
    return !!cert && cert.hash !== DO_NOT_CHECK_HASH &&
        cert.hash !== DEFAULT_HASH;
  }

  private getIsConfigured_(): boolean {
    if (this.securityType_ === undefined || !this.configProperties_) {
      return false;
    }

    const typeConfig = this.configProperties_.typeConfig;
    if (typeConfig.vpn) {
      if (this.vpnType_ === VPNConfigType.IKEV2 && !this.isIkev2Supported_()) {
        return false;
      }
      return this.vpnIsConfigured_();
    }

    if (typeConfig.wifi) {
      if (!typeConfig.wifi.ssid) {
        return false;
      }
      if (this.configRequiresPassphrase_) {
        const passphrase = typeConfig.wifi.passphrase;
        if (!passphrase || passphrase.length < MIN_PASSPHRASE_LENGTH) {
          return false;
        }
      }
    }
    if (this.securityType_ === SecurityType.kWpaEap) {
      return this.eapIsConfigured_();
    }
    return true;
  }

  private updateIsConfigured_(): void {
    this.isConfigured_ = this.getIsConfigured_();
  }

  private isWiFi_(networkType: NetworkType): boolean {
    return networkType === NetworkType.kWiFi;
  }

  private setEnableSave_(): void {
    this.enableSave = this.isConfigured_ && !!this.managedProperties_;
  }

  private setEnableConnect_(): void {
    this.enableConnect = this.isConfigured_ && !this.propertiesSent_;
  }

  private securityIsVisible_(networkType: NetworkType): boolean {
    return networkType === NetworkType.kWiFi ||
        networkType === NetworkType.kEthernet;
  }

  private securityIsEnabled_(): boolean {
    // WiFi Security type cannot be changed once configured.
    return !this.guid || this.mojoType_ === NetworkType.kEthernet;
  }

  private shareIsVisible_(): boolean {
    if (!this.managedProperties_) {
      return false;
    }
    return this.managedProperties_.source === OncSource.kNone &&
        this.managedProperties_.type === NetworkType.kWiFi;
  }

  private shareIsEnabled_(): boolean {
    if (!this.managedProperties_) {
      return false;
    }
    if (!this.shareAllowEnable ||
        this.managedProperties_.source !== OncSource.kNone) {
      return false;
    }
    return true;
  }

  /**
   * Returns true if the network configured by this UI element is ephemeral
   * according to enterprise policy.
   */
  private networkIsEphemeral_(): boolean {
    if (!loadTimeData.getBoolean('ephemeralNetworkPoliciesEnabled')) {
      return false;
    }
    if (!this.globalPolicy_ ||
        !this.globalPolicy_.userCreatedNetworkConfigurationsAreEphemeral) {
      return false;
    }
    if (!this.managedProperties_) {
      return false;
    }
    // Only user-created networks are ephemeral with this policy.
    return this.managedProperties_.source === OncSource.kNone;
  }

  private configCanAutoConnect_(): boolean {
    // Only WiFi can choose whether or not to autoConnect.
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        this.mojoType_ === NetworkType.kWiFi;
  }

  private autoConnectDisabled_(): boolean {
    return this.isAutoConnectEnforcedByPolicy_();
  }

  private isAutoConnectEnforcedByPolicy_(): boolean {
    return !!this.globalPolicy_ &&
        !!this.globalPolicy_.allowOnlyPolicyNetworksToAutoconnect;
  }

  private showHiddenNetworkWarning_(): boolean {
    flush();
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        this.autoConnect_ && !this.hasGuid_();
  }

  private updateHiddenNetworkWarning_(): void {
    this.hiddenNetworkWarning_ = this.showHiddenNetworkWarning_();
  }

  private selectedServerCaHashIsValid_(): boolean {
    return !!this.selectedServerCaHash_ &&
        this.selectedServerCaHash_ !== NO_CERTS_HASH;
  }

  private selectedUserCertHashIsValid_(): boolean {
    return !!this.selectedUserCertHash_ &&
        this.selectedUserCertHash_ !== NO_CERTS_HASH;
  }

  private eapIsConfigured_(): boolean {
    if (!this.configProperties_) {
      return false;
    }
    const eap = this.getEap_(this.configProperties_);
    if (!eap) {
      return false;
    }
    if (eap.outer !== 'EAP-TLS') {
      return true;
    }
    // EAP TLS networks can be shared only for device-wide certificates.
    if (this.deviceCertsOnly_) {  // network is shared
      let cert = this.findCert_(this.userCerts_, this.selectedUserCertHash_);
      if (!cert || !cert.deviceWide) {
        return false;
      }
      cert = this.findCert_(this.serverCaCerts_, this.selectedServerCaHash_);
      assert(cert);
      if (!cert.deviceWide) {
        return false;
      }
    }

    return this.selectedUserCertHashIsValid_();
  }

  private ikev2IsConfigured_(): boolean {
    assert(this.configProperties_);
    switch (this.ipsecAuthType_) {
      case IpsecAuthType.PSK: {
        const vpn = this.configProperties_.typeConfig.vpn;
        assert(vpn);
        assert(vpn.ipSec);
        return !!vpn.ipSec.psk;
      }
      case IpsecAuthType.CERT:
        // TODO(b/206722135): Show proper error message in the UI if server CA
        // is invalid.
        return this.selectedServerCaHashIsValid_() &&
            this.selectedUserCertHashIsValid_();
      case IpsecAuthType.EAP: {
        assert(this.eapProperties_);
        return this.selectedServerCaHashIsValid_() &&
            !!this.eapProperties_.identity;
      }
      default:
        assertNotReached();
    }
  }

  private l2tpIpsecIsConfigured_(): boolean {
    assert(this.configProperties_);
    const vpn = this.configProperties_.typeConfig.vpn;
    assert(vpn);
    switch (this.ipsecAuthType_) {
      case IpsecAuthType.PSK: {
        assert(vpn.l2tp);
        assert(vpn.ipSec);
        return !!vpn.l2tp.username && !!vpn.ipSec.psk;
      }
      case IpsecAuthType.CERT: {
        assert(vpn.l2tp);
        // TODO(b/206722135): Show proper error message in the UI if server CA
        // is invalid.
        return !!vpn.l2tp.username && this.selectedServerCaHashIsValid_() &&
            this.selectedUserCertHashIsValid_();
      }
      default:
        assertNotReached();
    }
  }

  private isValidWireGuardKey_(input: string|undefined|null): boolean {
    // A base64 translation of a 32 byte string is 44 bytes with '=' ending
    return !!input && input.length === 44 && input.charAt(43) === '=' &&
        !!input.match(/^[a-z0-9+/=]+$/i);
  }

  /**
   * Checks if the input ipAddresses is a comma-delimited string which contains
   * IP addresses (v4, v6, or both).
   */
  private isValidWireGuardIpAddresses_(ipAddresses: string|undefined): boolean {
    if (!ipAddresses) {
      return false;
    }
    // Currently shill only supports at most 1 IPv4 + 1 IPv6 address.
    let v4Count = 0;
    let v6Count = 0;
    for (const ipAddress of ipAddresses.split(',')) {
      if (ipAddress.match(IPV4_ADDR_REGEX)) {
        v4Count++;
      } else if (ipAddress.match(IPV6_ADDR_REGEX)) {
        v6Count++;
      } else {
        return false;
      }
    }
    if (v4Count > 1 || v6Count > 1) {
      return false;
    }
    return v4Count + v6Count > 0;
  }

  private isWireGuardConfigurationValid_(
      wireguard: WireGuardConfigProperties|null|undefined,
      ipAddresses: string|undefined): boolean {
    if (!wireguard) {
      return false;
    }
    if (!this.isValidWireGuardIpAddresses_(ipAddresses)) {
      return false;
    }
    if (this.isWireGuardUserPrivateKeyInputActive_ &&
        !this.isValidWireGuardKey_(wireguard.privateKey)) {
      return false;
    }
    // TODO: Current UI only supports configuring a single peer.
    assert(wireguard.peers);
    const peer = wireguard.peers[0];
    if (!this.isValidWireGuardKey_(peer.publicKey)) {
      return false;
    }
    if (!!peer.presharedKey && peer.presharedKey !== PLACEHOLDER_CREDENTIAL &&
        !this.isValidWireGuardKey_(peer.presharedKey)) {
      return false;
    }
    // endpoint should be the form of IP:port or hostname:port
    if (!peer.endpoint ||
        !peer.endpoint.match(/^\[?[a-zA-Z0-9\-\.:]+\]?:[0-9]+$/i)) {
      return false;
    }
    // allowedIps should be comma-separated list of IP/cidr.
    if (!peer.allowedIps ||
        !peer.allowedIps.split(',').every(s => s.match(IP_CIDR_REGEX))) {
      return false;
    }
    return true;
  }

  private vpnIsConfigured_(): boolean {
    assert(this.configProperties_);
    const vpn = this.configProperties_.typeConfig.vpn;
    if (!this.configProperties_.name || !vpn ||
        (!vpn.host && this.vpnType_ !== VPNConfigType.WIREGUARD)) {
      return false;
    }

    switch (this.vpnType_) {
      case VPNConfigType.IKEV2:
        return this.ikev2IsConfigured_();
      case VPNConfigType.L2TP_IPSEC:
        return this.l2tpIpsecIsConfigured_();
      case VPNConfigType.OPEN_VPN:
        // OpenVPN should require username + password OR a user cert. However,
        // there may be servers with different requirements so err on the side
        // of permissiveness.
        return true;
      case VPNConfigType.WIREGUARD:
        return this.isWireGuardConfigurationValid_(
            vpn.wireguard, this.ipAddressInput_);
    }
    return false;
  }

  private getPropertiesToSet_(): ConfigProperties {
    const propertiesToSet = Object.assign({}, this.configProperties_);
    // Do not set AutoConnect by default, the connection manager will set
    // it to true on a successful connection.
    propertiesToSet.autoConnect = null;
    if (this.guid) {
      propertiesToSet.guid = this.guid;
    }
    const eap = this.getEap_(propertiesToSet);
    if (eap) {
      this.setEapProperties_(eap);
    }
    if (this.mojoType_ === NetworkType.kVPN) {
      const vpnConfig = propertiesToSet.typeConfig.vpn;
      assert(vpnConfig);
      // VPN.Host can be an IP address but will not be recognized as such if
      // there is initial whitespace, so trim it.
      if (vpnConfig.host) {
        vpnConfig.host = vpnConfig.host.trim();
      }
      assert(vpnConfig.type);
      const vpnType = vpnConfig.type.value;

      if (vpnType === VpnType.kOpenVPN) {
        this.setOpenVPNProperties_(propertiesToSet);
      } else {
        assert(propertiesToSet.typeConfig.vpn);
        propertiesToSet.typeConfig.vpn.openVpn = null;
      }
      if (vpnType === VpnType.kIKEv2) {
        this.setVpnIkev2Properties_(propertiesToSet);
      } else if (vpnType === VpnType.kL2TPIPsec) {
        this.setVpnL2tpIpsecProperties_(propertiesToSet);
      } else {
        assert(propertiesToSet.typeConfig.vpn);
        propertiesToSet.typeConfig.vpn.ipSec = null;
        propertiesToSet.typeConfig.vpn.l2tp = null;
      }
      if (vpnType === VpnType.kWireGuard) {
        this.setWireGuardProperties_(propertiesToSet);
      } else {
        assert(propertiesToSet.typeConfig.vpn);
        propertiesToSet.typeConfig.vpn.wireguard = null;
      }
    }
    return propertiesToSet;
  }

  private getServerCaPems_(): string[] {
    const caHash = this.selectedServerCaHash_ || '';
    if (!caHash || caHash === DO_NOT_CHECK_HASH || caHash === DEFAULT_HASH) {
      return [];
    }
    const serverCa = this.findCert_(this.serverCaCerts_, caHash);
    return serverCa && serverCa.pemOrId ? [serverCa.pemOrId] : [];
  }

  private getUserCertPkcs11Id_(): string {
    const userCertHash = this.selectedUserCertHash_ || '';
    if (!this.selectedUserCertHashIsValid_() ||
        userCertHash === NO_USER_CERT_HASH) {
      return '';
    }
    const userCert = this.findCert_(this.userCerts_, userCertHash);
    return (userCert && userCert.pemOrId) || '';
  }

  private setEapProperties_(eap: EAPConfigProperties): void {
    eap.useSystemCas = this.selectedServerCaHash_ === DEFAULT_HASH;

    eap.serverCaPems = this.getServerCaPems_();

    const pkcs11Id = this.getUserCertPkcs11Id_();
    eap.clientCertType = pkcs11Id ? 'PKCS11Id' : 'None';
    eap.clientCertPkcs11Id = pkcs11Id || '';
  }

  private setVpnIkev2Properties_(propertiesToSet: ConfigProperties): void {
    assert(propertiesToSet.typeConfig.vpn);
    const ipsec = propertiesToSet.typeConfig.vpn.ipSec;
    assert(!!ipsec);

    ipsec.authenticationType = this.ipsecAuthType_;
    if (ipsec.authenticationType !== IpsecAuthType.PSK) {
      // Set psk to empty string to make sure the value is cleared.
      ipsec.psk = '';
      // For non-PSK auth method, server CA is mandatory.
      ipsec.serverCaPems = this.getServerCaPems_();
    }

    if (ipsec.authenticationType === IpsecAuthType.CERT) {
      ipsec.clientCertType = 'PKCS11Id';
      ipsec.clientCertPkcs11Id = this.getUserCertPkcs11Id_();
    } else {
      ipsec.clientCertType = null;
      ipsec.clientCertPkcs11Id = null;
    }

    if (ipsec.authenticationType === IpsecAuthType.EAP) {
      // Not all fields in eap are used by IKEv2, so create a new object here.
      const eap = ipsec.eap;
      assert(eap);
      ipsec.eap = {
        domainSuffixMatch: [],
        identity: eap.identity,
        outer: 'MSCHAPv2',
        password: eap.password,
        saveCredentials: this.vpnSaveCredentials_,
        subjectAltNameMatch: [],
        useSystemCas: false,
        anonymousIdentity: null,
        clientCertPkcs11Id: null,
        clientCertType: null,
        inner: null,
        serverCaPems: null,
        subjectMatch: null,
      };
    } else {
      ipsec.eap = null;
    }

    ipsec.ikeVersion = 2;
    ipsec.saveCredentials = this.vpnSaveCredentials_;
  }

  private setOpenVPNProperties_(propertiesToSet: ConfigProperties): void {
    assert(propertiesToSet.typeConfig.vpn);
    const openVpn = propertiesToSet.typeConfig.vpn.openVpn;
    assert(!!openVpn);

    openVpn.serverCaPems = this.getServerCaPems_();

    const pkcs11Id = this.getUserCertPkcs11Id_();
    openVpn.clientCertType = pkcs11Id ? 'PKCS11Id' : 'None';
    openVpn.clientCertPkcs11Id = pkcs11Id || '';

    if (openVpn.password) {
      openVpn.userAuthenticationType =
          openVpn.otp ? 'PasswordAndOTP' : 'Password';
    } else if (openVpn.otp) {
      openVpn.userAuthenticationType = 'OTP';
    } else {
      openVpn.userAuthenticationType = 'None';
    }

    openVpn.saveCredentials = this.vpnSaveCredentials_;
    propertiesToSet.typeConfig.vpn.openVpn = openVpn;
  }

  private setWireGuardProperties_(propertiesToSet: ConfigProperties): void {
    assert(propertiesToSet.typeConfig.vpn);
    const wireguard = propertiesToSet.typeConfig.vpn.wireguard;
    assert(!!wireguard);
    propertiesToSet.typeConfig.vpn.host = 'wireguard';
    propertiesToSet.ipAddressConfigType = 'Static';
    assert(this.ipAddressInput_);
    wireguard.ipAddresses = this.ipAddressInput_.split(',');
    propertiesToSet.staticIpConfig = {
      gateway: this.ipAddressInput_,
      routingPrefix: 32,
      type: IPConfigType.kIPv4,
      ipAddress: null,
      excludedRoutes: null,
      includedRoutes: null,
      nameServers: null,
      searchDomains: null,
      webProxyAutoDiscoveryUrl: null,
    };
    if (this.nameServersInput_) {
      propertiesToSet.nameServersConfigType = 'Static';
      propertiesToSet.staticIpConfig.nameServers =
          this.nameServersInput_.split(',');
    }
    if (this.wireguardKeyType_ === WireGuardKeyConfigType.USE_CURRENT) {
      wireguard.privateKey = null;
    } else if (this.wireguardKeyType_ === WireGuardKeyConfigType.GENERATE_NEW) {
      wireguard.privateKey = '';
    }
    assert(!!wireguard.peers);
    for (const peer of wireguard.peers) {
      if (peer.presharedKey === PLACEHOLDER_CREDENTIAL) {
        peer.presharedKey = null;  // No modification
      } else if (peer.presharedKey === undefined) {
        peer.presharedKey = '';  // Explicitly removed
      }
    }
  }

  private setVpnL2tpIpsecProperties_(propertiesToSet: ConfigProperties) {
    const vpn = propertiesToSet.typeConfig.vpn;
    assert(vpn);
    assert(vpn.ipSec);
    assert(vpn.l2tp);

    vpn.ipSec.authenticationType = this.ipsecAuthType_;
    if (vpn.ipSec.authenticationType === IpsecAuthType.CERT) {
      vpn.ipSec.clientCertType = 'PKCS11Id';
      vpn.ipSec.clientCertPkcs11Id = this.getUserCertPkcs11Id_();
      vpn.ipSec.serverCaPems = this.getServerCaPems_();
    }
    vpn.ipSec.ikeVersion = 1;
    vpn.ipSec.saveCredentials = this.vpnSaveCredentials_;
    vpn.l2tp.saveCredentials = this.vpnSaveCredentials_;

    // Clear IPsec fields which are only for IKEv2.
    vpn.ipSec.eap = null;
    vpn.ipSec.localIdentity = null;
    vpn.ipSec.remoteIdentity = null;
  }

  /**
   * @param connect If true, connect after save.
   */
  private setPropertiesCallback_(
      success: boolean, errorMessage: string, connect: boolean): void {
    if (!success) {
      console.warn(
          'Unable to set properties for: ' + this.guid +
          ' Error: ' + errorMessage);
      this.propertiesSent_ = false;
      this.setError_(errorMessage);
      this.focusPassphrase_();
      return;
    }

    assert(this.managedProperties_);
    // Only attempt a connection if the network is not yet connected.
    if (connect &&
        this.managedProperties_.connectionState ===
            ConnectionStateType.kNotConnected) {
      this.startConnect_(this.guid);
    } else {
      this.close_();
    }
  }

  private createNetworkCallback_(
      guid: string|null, errorMessage: string, connect: boolean): void {
    if (!guid) {
      console.warn(
          'Unable to configure network: ' + guid + ' Error: ' + errorMessage);
      this.propertiesSent_ = false;
      this.setError_(errorMessage);
      this.focusPassphrase_();
      return;
    }

    if (connect) {
      this.startConnect_(guid);
    } else {
      this.close_();
    }
  }

  private startConnect_(guid: string): void {
    this.networkConfig_.startConnect(guid).then(response => {
      const result = response.result;
      if (result === StartConnectResult.kSuccess ||
          result === StartConnectResult.kInvalidGuid ||
          result === StartConnectResult.kInvalidState ||
          result === StartConnectResult.kCanceled) {
        // Connect succeeded, or is in progress completed or canceled.
        // Close the dialog.
        this.close_();
        return;
      }
      this.setError_(response.message);
      console.warn(
          'Error connecting to network: ' + guid + ': ' + result.toString() +
          ' Message: ' + response.message);
      this.propertiesSent_ = false;
    });
  }

  private computeConfigRequiresPassphrase_(
      mojoType: NetworkType|undefined,
      securityType: SecurityType|undefined): boolean {
    // Note: 'Passphrase' is only used by WiFi; Ethernet uses EAP.Password.
    return mojoType === NetworkType.kWiFi &&
        (securityType === SecurityType.kWepPsk ||
         securityType === SecurityType.kWpaPsk);
  }

  private getEapInnerItems_(outer: string): string[] {
    if (outer === 'PEAP') {
      return this.eapInnerItemsPeap_;
    }
    if (outer === 'EAP-TTLS') {
      return this.eapInnerItemsTtls_;
    }
    return [];
  }

  private setError_(error: string|null): void {
    this.error = error || '';
  }

  private getManagedSecurity_(managedProperties: ManagedProperties):
      ManagedString|undefined {
    const policySource =
        OncMojo.getEnforcedPolicySourceFromOncSource(managedProperties.source);
    if (policySource === PolicySource.kNone) {
      return undefined;
    }
    switch (managedProperties.type) {
      case NetworkType.kWiFi: {
        assert(managedProperties.typeProperties.wifi);
        return {
          activeValue: OncMojo.getSecurityTypeString(
              managedProperties.typeProperties.wifi.security),
          policySource: policySource,
          policyValue: null,
        };
      }
      case NetworkType.kEthernet: {
        assert(managedProperties.typeProperties.ethernet);
        return {
          activeValue: OncMojo.getActiveString(
              managedProperties.typeProperties.ethernet.authentication),
          policySource: policySource,
          policyValue: null,
        };
      }
    }
    return undefined;
  }

  private getManagedVpnSaveCredentials_(managedProperties: ManagedProperties):
      ManagedBoolean|undefined|null {
    const vpn = managedProperties.typeProperties.vpn;
    assert(vpn);
    switch (vpn.type) {
      case VpnType.kIKEv2: {
        assert(vpn.ipSec);
        return vpn.ipSec.saveCredentials || OncMojo.createManagedBool(false);
      }
      case VpnType.kOpenVPN: {
        assert(vpn.openVpn);
        return vpn.openVpn.saveCredentials || OncMojo.createManagedBool(false);
      }
      case VpnType.kL2TPIPsec: {
        assert(vpn.ipSec);
        assert(vpn.l2tp);
        return vpn.ipSec.saveCredentials || vpn.l2tp.saveCredentials ||
            OncMojo.createManagedBool(false);
      }
      case VpnType.kWireGuard:
        return OncMojo.createManagedBool(true);
    }
    assertNotReached();
  }

  private getManagedVpnServerCaRefs_(managedProperties: ManagedProperties):
      ManagedStringList|undefined|null {
    const vpn = managedProperties.typeProperties.vpn;
    assert(vpn);
    switch (vpn.type) {
      case VpnType.kOpenVPN:
        assert(vpn.openVpn);
        return vpn.openVpn.serverCaRefs;
      case VpnType.kIKEv2:
      case VpnType.kL2TPIPsec:
        assert(vpn.ipSec);
        return vpn.ipSec.serverCaRefs;
    }
    assertNotReached();
  }

  private getManagedVpnClientCertType_(managedProperties: ManagedProperties):
      ManagedString|undefined {
    const vpn = managedProperties.typeProperties.vpn;
    assert(vpn);
    switch (vpn.type) {
      case VpnType.kOpenVPN:
        assert(vpn.openVpn);
        return vpn.openVpn.clientCertType || OncMojo.createManagedString('');
      case VpnType.kIKEv2:
      case VpnType.kL2TPIPsec:
        assert(vpn.ipSec);
        return vpn.ipSec.clientCertType || OncMojo.createManagedString('');
    }
    assertNotReached();
  }

  private onWifiPasswordInputKeypress_() {
    // bad-passphrase corresponds to kErrorBadPassphrase in shill
    if (this.error === 'bad-passphrase') {
      // Reset error if user starts typing new password.
      this.setError_('');
    }
  }

  /**
   * Verifies if the selected server CA certificate can be used for the selected
   * EAP method. This method returns false is the selected EAP method requires a
   * server CA certificate and the user selected the default certificate without
   * configuring the domain suffix match or subject alternative match and
   * without explicitly allowing insecure connections via Chrome flags.
   * Otherwise returns true.
   */
  private eapConfigServerCaCertAllowed_(): boolean {
    assert(this.eapProperties_);

    const outer = this.eapProperties_.outer;
    if (!(outer === 'EAP-TLS' || outer === 'EAP-TTLS' || outer === 'PEAP')) {
      return true;
    }

    if (this.selectedServerCaHash_ !== DEFAULT_HASH) {
      // Does not use default CA server certs.
      return true;
    }

    const isPropertyManaged = !!this.managedEapProperties_ &&
        !!this.managedEapProperties_.useSystemCas &&
        (this.managedEapProperties_.useSystemCas.policySource !==
         PolicySource.kNone);
    // Bypass `domainSuffixMatch` and `subjectAltNameMatch` checks for managed
    // networks if the user doesn't control the CA setting.
    if (isPropertyManaged) {
      return true;
    }

    if (this.eapProperties_.domainSuffixMatch.length != 0 ||
        this.eapProperties_.subjectAltNameMatch.length != 0) {
      return true;
    }

    return false;
  }

  // Force |securityType_| to an enum value when the <select> element sets it
  // to a string. See crbug.com/1046149 for details.
  private getSecurityTypeAsNumber(securityType: SecurityType|
                                  string): SecurityType {
    if (typeof this.securityType_ === 'string') {
      return Number.parseInt(this.securityType_, 10);
    }

    return securityType as SecurityType;
  }

  getSecurityTypeForTesting(): SecurityType {
    return this.securityType_ as SecurityType;
  }

  setSecurityTypeForTesting(securityType: SecurityType): void {
    this.securityType_ = securityType;
  }

  getShareNetworkForTesting(): boolean {
    return this.shareNetwork_;
  }

  setShareNetworkForTesting(shareNetwork: boolean): void {
    this.shareNetwork_ = shareNetwork;
  }

  getPropertiesSentForTesting(): boolean {
    return this.propertiesSent_;
  }

  getManagedPropertiesForTesting(): ManagedProperties {
    assert(this.managedProperties_);
    return this.managedProperties_;
  }

  getPropertiesToSetForTesting(): ConfigProperties {
    return this.getPropertiesToSet_();
  }

  setManagedPropertiesForTesting(managedProperties: ManagedProperties): void {
    this.managedProperties_ = managedProperties;
  }

  setSerializedSubjectAltNameMatchForTesting(serializedSubjectAltNameMatch:
                                                 string): void {
    this.serializedSubjectAltNameMatch_ = serializedSubjectAltNameMatch;
  }

  setSerializedDomainSuffixMatchForTesting(serializedDomainSuffixMatch: string):
      void {
    this.serializedDomainSuffixMatch_ = serializedDomainSuffixMatch;
  }

  getVpnIsConfiguredForTesting(): boolean {
    return this.vpnIsConfigured_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkConfigElement.is]: NetworkConfigElement;
  }
}

customElements.define(NetworkConfigElement.is, NetworkConfigElement);

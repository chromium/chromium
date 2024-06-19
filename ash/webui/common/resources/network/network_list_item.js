// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list based on ONC state properties.
 */

import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './network_icon.js';

import {assert} from '//resources/ash/common/assert.js';
import {CellularSetupPageName} from '//resources/ash/common/cellular_setup/cellular_types.js';
import {getESimProfileProperties} from '//resources/ash/common/cellular_setup/esim_manager_utils.js';
import {CrPolicyIndicatorType} from '//resources/ash/common/cr_policy_indicator_behavior.js';
import {FocusRowBehavior} from '//resources/ash/common/focus_row_behavior.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {ActivationStateType, CrosNetworkConfigInterface, GlobalPolicy, SecurityType, VpnType} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType, OncSource, PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {getTemplate} from './network_list_item.html.js';
import {NetworkList} from './network_list_types.js';
import {OncMojo} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-list-item',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
    FocusRowBehavior,
  ],

  properties: {
    /**
     * Dims the UI, disables click and keyboard event handlers.
     * @private
     */
    disabled_: {
      type: Boolean,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
      computed: 'computeDisabled_(deviceState, deviceState.inhibitReason,' +
          'disableItem, isUserLoggedIn_, isPSimPendingActivationNetwork_,' +
          'isBuiltInVpnManagementBlocked, networkState,' +
          'networkState.typeState.vpn, networkState.typeState.vpn.type)',
    },

    /**
     * Set by network-list to force disable this network item.
     * @type {boolean}
     */
    disableItem: Boolean,

    isBuiltInVpnManagementBlocked: {
      type: Boolean,
      value: false,
    },

    /** @type {!NetworkList.NetworkListItemType|undefined} */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    /**
     * The ONC data properties used to display the list item.
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
    },

    /** Whether to show any buttons for network items. Defaults to false. */
    showButtons: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Reflect the element's tabindex attribute to a property so that embedded
     * elements (e.g. the show subpage button) can become keyboard focusable
     * when this element has keyboard focus.
     */
    tabindex: {
      type: Number,
      value: -1,
    },

    /**
     * Expose the itemName so it can be used as a label for a11y.  It will be
     * added as an attribute on this top-level network-list-item, and can
     * be used by any sub-element which applies it.
     */
    rowLabel: {
      type: String,
      notify: true,
      computed:
          'getRowLabel_(item, networkState, subtitle_, isPSimPendingActivationNetwork_)',
    },

    buttonLabel: {
      type: String,
      computed: 'getButtonLabel_(item)',
    },

    /**
     * The cached ConnectionState for the network.
     * @type {!ConnectionStateType|undefined}
     */
    connectionState_: Number,

    /** Whether to show technology badge on mobile network icon. */
    showTechnologyBadge: {
      type: Boolean,
      value: true,
    },

    /** Whether cellular activation is unavailable in the current context. */
    activationUnavailable: {
      type: Boolean,
      value: false,
    },

    /**
     * DeviceState associated with the network item type, or undefined if none
     * was provided.
     * @private {!OncMojo.DeviceStateProperties|undefined} deviceState
     */
    deviceState: Object,

    /** @type {!GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * WARNING: This string may contain malicious HTML and should not be used
     * for Polymer bindings in CSS code. For additional information see
     * b/286254915.
     *
     * Title containing the item's name and subtitle.
     * @private {string}
     */
    itemTitle_: {
      type: String,
      value: '',
    },

    /**
     * Subtitle for item.
     * @private {string}
     */
    subtitle_: {
      type: String,
      value: '',
    },

    /**
     * Indicates the network item is a pSIM network not yet activated but
     * eligible for activation.
     * @private
     */
    isPSimPendingActivationNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimPendingActivationNetwork_(networkState.*)',
    },

    /**
     * Indicates the network item is a pSIM network that is not activated nor
     * available to be activated.
     * @private
     */
    isPSimUnavailableNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimUnavailableNetwork_(networkState.*)',
    },

    /**
     * Indicates the network item is a pSIM network currently activating.
     * @private
     */
    isPSimActivatingNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimActivatingNetwork_(networkState.*)',
    },

    /**
     * Whether the network item is a cellular one and is of an esim
     * pending profile.
     * @private
     */
    isESimPendingProfile_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsESimPendingProfile_(item, item.customItemType)',
    },

    /**
     * Whether the network item is a cellular one and is of an esim
     * installing profile.
     * @private
     */
    isESimInstallingProfile_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsESimInstallingProfile_(item, item.customItemType)',
    },

    /** @private */
    isESimUnactivatedProfile_: {
      type: Boolean,
      value: false,
      computed: 'computeIsESimUnactivatedProfile_(networkState.*)',
    },

    /**
     * Indicates the network item is a blocked cellular network by policy.
     * @private
     */
    isBlockedNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsBlockedNetwork_(item, globalPolicy)',
    },

    /**@private {boolean} */
    isCellularUnlockDialogOpen_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isUserLoggedIn_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('isUserLoggedIn') &&
            loadTimeData.getBoolean('isUserLoggedIn');
      },
    },
  },

  /** @private {?CrosNetworkConfigInterface} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  },

  /** @override */
  attached() {
    this.listen(this, 'keydown', 'onKeydown_');
  },

  /** @override */
  detached() {
    this.unlisten(this, 'keydown', 'onKeydown_');
  },

  /**
   * @return {boolean}
   * @private
   */
  isESimNetwork_() {
    return !!this.networkState &&
        this.networkState.type === NetworkType.kCellular &&
        !!this.networkState.typeState.cellular &&
        !!this.networkState.typeState.cellular.eid &&
        !!this.networkState.typeState.cellular.iccid;
  },

  /**
   * @return {boolean}
   * @private
   */
  isPSimNetwork_() {
    return !!this.networkState &&
        this.networkState.type === NetworkType.kCellular &&
        !!this.networkState.typeState.cellular &&
        !this.networkState.typeState.cellular.eid &&
        !!this.networkState.typeState.cellular.iccid;
  },

  /**
   * @return {boolean}
   * @private
   */
  isBuiltInVpn_() {
    if (!this.networkState || this.networkState.type !== NetworkType.kVPN) {
      return false;
    }

    const vpnType = this.networkState.typeState.vpn.type;
    return vpnType === VpnType.kL2TPIPsec || vpnType === VpnType.kOpenVPN;
  },

  /** @private */
  async itemChanged_() {
    if (this.item && !this.item.hasOwnProperty('customItemType')) {
      this.networkState =
          /** @type {!OncMojo.NetworkStateProperties} */ (this.item);
    } else {
      this.networkState = undefined;
    }
    // The order each property is set here matters. We don't use observers to
    // set each property or else the ordering is indeterminate.
    await this.setSubtitle_();
    this.setItemTitle_();
  },

  /** @private */
  async setSubtitle_() {
    if (this.item.hasOwnProperty('customItemSubtitle') &&
        this.item.customItemSubtitle) {
      // Item is a custom OOBE network or pending eSIM profile.
      const item = /** @type {!NetworkList.CustomItemState} */ (this.item);
      this.subtitle_ = item.customItemSubtitle;
      return;
    }

    // Clear subtitle to ensure that stale values are not displayed when this
    // component is recycled for a case without subtitles.
    this.subtitle_ = '';

    // Show service provider subtext only when networkState is an eSIM cellular
    // network.
    if (!this.isESimNetwork_()) {
      return;
    }

    const properties = await getESimProfileProperties(
        this.networkState.typeState.cellular.iccid);
    if (!properties) {
      return;
    }

    // The parent list component could recycle the same component to show
    // different networks. So networkState could have changed while the async
    // operations above were in progress. Skip updating subtitle if network
    // state does not match the fetched eSIM profile.
    if (!this.networkState || !this.networkState.typeState.cellular ||
        this.networkState.typeState.cellular.iccid !== properties.iccid) {
      return;
    }

    // Service provider from mojo API is a string16 value represented as an
    // array of characters. Convert to string for display.
    this.subtitle_ = mojoString16ToString(properties.serviceProvider);
  },

  /** @private */
  networkStateChanged_() {
    if (!this.networkState) {
      return;
    }

    const connectionState = this.networkState.connectionState;
    if (connectionState === this.connectionState_) {
      return;
    }
    this.connectionState_ = connectionState;
    this.fire('network-connect-changed', this.networkState);
  },

  /** @private */
  setItemTitle_() {
    const itemName = this.getItemName_();
    const subtitle = this.getSubtitle();
    if (!subtitle || (this.isESimNetwork_() && itemName === subtitle)) {
      this.itemTitle_ = itemName;
      return;
    }
    this.itemTitle_ =
        loadTimeData.getStringF('networkListItemTitle', itemName, subtitle);
  },

  /**
   * WARNING: The string returned by this method may contain malicious HTML and
   * should not be used for Polymer bindings in CSS code. For additional
   * information see b/286254915.
   *
   * This gets called for network items and custom items.
   * @return {string}
   * @private
   */
  getItemName_() {
    if (this.item.hasOwnProperty('customItemName')) {
      const item = /** @type {!NetworkList.CustomItemState} */ (this.item);
      return this.i18nExists(item.customItemName) ?
          this.i18n(item.customItemName) :
          item.customItemName;
    }
    return OncMojo.getNetworkStateDisplayNameUnsafe(
        /** @type {!OncMojo.NetworkStateProperties} */ (this.item));
  },

  /**
   * The aria label for the subpage button.
   * @return {string}
   * @private
   */
  getButtonLabel_() {
    return loadTimeData.getStringF(
        'networkListItemSubpageButtonLabel', this.getItemName_());
  },

  /**
   * @return {boolean}
   * @private
   */
  computeDisabled_() {
    if (this.disableItem) {
      return true;
    }
    if (this.isBuiltInVpn_() && this.isBuiltInVpnManagementBlocked) {
      return true;
    }
    if (!this.deviceState) {
      return false;
    }
    if (this.isPsimPendingActivationWhileLoggedOut_()) {
      return true;
    }

    return OncMojo.deviceIsInhibited(this.deviceState);
  },

  /**
   * Label for the row, used for accessibility announcement.
   * @return {string}
   * @private
   */
  getRowLabel_() {
    if (!this.item) {
      return '';
    }

    const sublabel = this.getSublabelText_();
    const isManaged = this.item.source === OncSource.kDevicePolicy ||
        this.item.source === OncSource.kUserPolicy;

    // TODO(jonmann): Reaching into the parent element breaks encapsulation so
    // refactor this logic into the parent (NetworkList) and pass into
    // NetworkListItem as a property.
    let index;
    let total;
    if (this.parentElement.items) {
      index = this.parentElement.items.indexOf(this.item) + 1;
      total = this.parentElement.items.length;
    } else {
      // This should only happen in tests; see TODO above.
      index = 0;
      total = 1;
    }

    switch (this.item.type) {
      case NetworkType.kCellular:
        if (isManaged) {
          if (sublabel) {
            if (this.subtitle_) {
              if (this.isPsimPendingActivationWhileLoggedOut_()) {
                return loadTimeData.getStringF(
                    'networkListItemLabelManagedActivateAfterSetupWithProviderName',
                    index, total, this.getItemName_(), this.subtitle_);
              }
              return loadTimeData.getStringF(
                  'networkListItemLabelCellularManagedWithConnectionStatusAndProviderName',
                  index, total, this.getItemName_(), this.subtitle_, sublabel,
                  this.item.typeState.cellular.signalStrength);
            }
            if (this.isPsimPendingActivationWhileLoggedOut_()) {
              return loadTimeData.getStringF(
                  'networkListItemLabelManagedActivateAfterSetup', index, total,
                  this.getItemName_());
            }
            return loadTimeData.getStringF(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), sublabel,
                this.item.typeState.cellular.signalStrength);
          }
          if (this.subtitle_) {
            return loadTimeData.getStringF(
                'networkListItemLabelCellularManagedWithProviderName', index,
                total, this.getItemName_(), this.subtitle_,
                this.item.typeState.cellular.signalStrength);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelCellularManaged', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }
        if (sublabel) {
          if (this.subtitle_) {
            if (this.isPsimPendingActivationWhileLoggedOut_()) {
              return loadTimeData.getStringF(
                  'networkListItemLabelActivateAfterSetupWithProviderName',
                  index, total, this.getItemName_(), this.subtitle_);
            }
            return loadTimeData.getStringF(
                'networkListItemLabelCellularWithConnectionStatusAndProviderName',
                index, total, this.getItemName_(), this.subtitle_, sublabel,
                this.item.typeState.cellular.signalStrength);
          }
          if (this.isPsimPendingActivationWhileLoggedOut_()) {
            return loadTimeData.getStringF(
                'networkListItemLabelActivateAfterSetup', index, total,
                this.getItemName_());
          }
          if (this.isPSimPendingActivationNetwork_) {
            return loadTimeData.getStringF(
                'networkListItemLabelCellularUnactivatedWithConnectionStatus',
                index, total, this.getItemName_(), sublabel,
                this.item.typeState.cellular.signalStrength);
          }
          if (this.isBlockedNetwork_) {
            return loadTimeData.getStringF(
                'networkListItemCellularBlockedWithConnectionStatusA11yLabel',
                index, total, this.getItemName_(), sublabel,
                this.item.typeState.cellular.signalStrength);
          }

          return loadTimeData.getStringF(
              'networkListItemLabelCellularWithConnectionStatus', index, total,
              this.getItemName_(), sublabel,
              this.item.typeState.cellular.signalStrength);
        }

        if (this.isPSimPendingActivationNetwork_) {
          return loadTimeData.getStringF(
              'networkListItemLabelCellularUnactivated', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }

        if (this.isBlockedNetwork_) {
          return loadTimeData.getStringF(
              'networkListItemCellularBlockedA11yLabel', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }

        if (this.subtitle_) {
          return loadTimeData.getStringF(
              'networkListItemLabelCellularWithProviderName', index, total,
              this.getItemName_(), this.subtitle_,
              this.item.typeState.cellular.signalStrength);
        }
        return loadTimeData.getStringF(
            'networkListItemLabelCellular', index, total, this.getItemName_(),
            this.item.typeState.cellular.signalStrength);
      case NetworkType.kEthernet:
        if (isManaged) {
          if (sublabel) {
            return loadTimeData.getStringF(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), sublabel);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelEthernetManaged', index, total,
              this.getItemName_());
        }
        if (sublabel) {
          return loadTimeData.getStringF(
              'networkListItemLabelEthernetWithConnectionStatus', index, total,
              this.getItemName_(), sublabel);
        }
        return loadTimeData.getStringF(
            'networkListItemLabel', index, total, this.getItemName_());
      case NetworkType.kTether:
        // Tether networks will never be controlled by policy (only disabled).
        if (sublabel) {
          if (this.subtitle_) {
            return loadTimeData.getStringF(
                'networkListItemLabelTetherWithConnectionStatusAndProviderName',
                index, total, this.getItemName_(), this.subtitle_, sublabel,
                this.item.typeState.tether.signalStrength,
                this.item.typeState.tether.batteryPercentage);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelTetherWithConnectionStatus', index, total,
              this.getItemName_(), sublabel,
              this.item.typeState.tether.signalStrength,
              this.item.typeState.tether.batteryPercentage);
        }
        if (this.subtitle_) {
          return loadTimeData.getStringF(
              'networkListItemLabelTetherWithProviderName', index, total,
              this.getItemName_(), this.subtitle_,
              this.item.typeState.tether.signalStrength,
              this.item.typeState.tether.batteryPercentage);
        }
        return loadTimeData.getStringF(
            'networkListItemLabelTether', index, total, this.getItemName_(),
            this.item.typeState.tether.signalStrength,
            this.item.typeState.tether.batteryPercentage);
      case NetworkType.kWiFi:
        const secured =
            this.item.typeState.wifi.security === SecurityType.kNone ?
            this.i18n('wifiNetworkStatusUnsecured') :
            this.i18n('wifiNetworkStatusSecured');
        if (isManaged) {
          if (sublabel) {
            return loadTimeData.getStringF(
                'networkListItemLabelWifiManagedWithConnectionStatus', index,
                total, this.getItemName_(), secured, sublabel,
                this.item.typeState.wifi.signalStrength);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelWifiManaged', index, total,
              this.getItemName_(), secured,
              this.item.typeState.wifi.signalStrength);
        }
        if (sublabel) {
          if (this.isBlockedNetwork_) {
            return loadTimeData.getStringF(
                'networkListItemWiFiBlockedWithConnectionStatusA11yLabel',
                index, total, this.getItemName_(), secured, sublabel,
                this.item.typeState.wifi.signalStrength);
          }

          return loadTimeData.getStringF(
              'networkListItemLabelWifiWithConnectionStatus', index, total,
              this.getItemName_(), secured, sublabel,
              this.item.typeState.wifi.signalStrength);
        }

        if (this.isBlockedNetwork_) {
          return loadTimeData.getStringF(
              'networkListItemWiFiBlockedA11yLabel', index, total,
              this.getItemName_(), secured,
              this.item.typeState.wifi.signalStrength);
        }

        return loadTimeData.getStringF(
            'networkListItemLabelWifi', index, total, this.getItemName_(),
            secured, this.item.typeState.wifi.signalStrength);
      default:
        if (this.isESimPendingProfile_) {
          if (this.subtitle_) {
            return loadTimeData.getStringF(
                'networkListItemLabelESimPendingProfileWithProviderName', index,
                total, this.getItemName_(), this.subtitle_);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelESimPendingProfile', index, total,
              this.getItemName_());
        } else if (this.isESimInstallingProfile_) {
          if (this.subtitle_) {
            return loadTimeData.getStringF(
                'networkListItemLabelESimPendingProfileWithProviderNameInstalling',
                index, total, this.getItemName_(), this.subtitle_);
          }
          return loadTimeData.getStringF(
              'networkListItemLabelESimPendingProfileInstalling', index, total,
              this.getItemName_());
        }
        return loadTimeData.getStringF(
            'networkListItemLabel', index, total, this.getItemName_());
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isSublabelVisible_() {
    return !!this.networkState && !!this.getSublabelText_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @return {string}
   * @private
   */
  getSublabelText_() {
    if (!this.networkState) {
      return '';
    }

    if (this.networkState.type === NetworkType.kCellular) {
      // For carrier lock, display string is different from regular
      // pin lock
      if (this.networkState.typeState.cellular.simLocked) {
        if (this.networkState.typeState.cellular.simLockType ===
            'network-pin') {
          return this.i18n(
              'networkListItemUpdatedCellularSimCardCarrierLocked');
        }
        return this.i18n('networkListItemUpdatedCellularSimCardLocked');
      }
      if (this.isPsimPendingActivationWhileLoggedOut_()) {
        return this.i18n('networkListItemActivateAfterDeviceSetup');
      }
      if (this.isPSimUnavailableNetwork_ || this.isESimUnactivatedProfile_) {
        return this.i18n('networkListItemUnavailableSimNetwork');
      }
    }

    const connectionState = this.networkState.connectionState;
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      if (this.isPortalState_(this.networkState.portalState)) {
        if (this.networkState.type === NetworkType.kCellular) {
          return this.i18n('networkListItemCellularSignIn');
        }
        return this.i18n('networkListItemSignIn');
      }
      if (this.networkState.portalState === PortalState.kNoInternet) {
        return this.i18n('networkListItemConnectedNoConnectivity');
      }
      // TODO(khorimoto): Consider differentiating between Connected and Online.
      return this.i18n('networkListItemConnected');
    }
    if (connectionState === ConnectionStateType.kConnecting) {
      return this.i18n('networkListItemConnecting');
    }
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getSublabelClass_() {
    if (this.shouldShowWarningState_()) {
      return 'warning';
    }
    return 'cr-secondary-text';
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowWarningState_() {
    // Warning is shown when a cellular SIM is locked, since connectivity is not
    // available in this state.
    if (this.networkState && this.networkState.type === NetworkType.kCellular &&
        this.networkState.typeState.cellular.simLocked) {
      return true;
    }

    // Warning is shown when a PSim is unavailable on current network or ESim is
    // unactivated on current network.
    if (this.isPSimUnavailableNetwork_ || this.isESimUnactivatedProfile_) {
      return true;
    }

    if (this.isPsimPendingActivationWhileLoggedOut_()) {
      return true;
    }

    // Warning is shown when there is restricted connectivity.
    if (this.networkState &&
        OncMojo.isRestrictedConnectivity(this.networkState.portalState)) {
      return true;
    }

    return false;
  },

  /**
   * @return {string}
   * @private
   */
  getSubtitle() {
    return this.subtitle_ ? this.subtitle_ : '';
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
   * @param {boolean} showButtons
   * @return {boolean}
   * @private
   */
  isSubpageButtonVisible_(networkState, showButtons, disabled_) {
    if (!this.showButtons) {
      return false;
    }
    if (this.isPSimPendingActivationNetwork_ || this.isPSimActivatingNetwork_) {
      return true;
    }
    if (this.isBuiltInVpn_() && this.isBuiltInVpnManagementBlocked) {
      return true;
    }
    return !!networkState && !disabled_ && !this.shouldShowUnlockButton_();
  },

  /**
   * @return {boolean} Whether this element's contents describe an "active"
   *     network. In this case, an active network is connected and may have
   *     additional properties (e.g., must be activated for cellular networks).
   * @private
   */
  isSublabelActive_() {
    if (!this.networkState) {
      return false;
    }
    if (this.shouldShowNotAvailableText_()) {
      return false;
    }
    if (this.isPsimPendingActivationWhileLoggedOut_()) {
      return false;
    }
    if (this.isESimUnactivatedProfile_) {
      return false;
    }
    if (OncMojo.isRestrictedConnectivity(this.networkState.portalState)) {
      return false;
    }
    return OncMojo.connectionStateIsConnected(
        this.networkState.connectionState);
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.onSelected_(event);

    // The default event for pressing Enter on a focused button is to simulate a
    // click on the button. Prevent this action, since it would navigate a
    // second time to the details page and cause an unnecessary entry to be
    // added to the back stack. See https://crbug.com/736963.
    event.preventDefault();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelected_(event) {
    if (this.disabled_) {
      event.stopImmediatePropagation();
      return;
    }
    if (this.isSubpageButtonVisible_(
            this.networkState, this.showButtons, this.disabled_) &&
        this.$$('#subpageButton') === this.shadowRoot.activeElement) {
      this.fireShowDetails_(event);
    } else if (this.shouldShowInstallButton_()) {
      this.onInstallButtonClick_(event);
    } else if (this.shouldShowUnlockButton_()) {
      this.onUnlockButtonClick_();
    } else if (this.item && this.item.hasOwnProperty('customItemName')) {
      this.fire('custom-item-selected', this.item);
    } else if (this.shouldShowActivateButton_()) {
      this.fireShowDetails_(event);
    } else if (
        this.showButtons &&
        (this.isPSimUnavailableNetwork_ || this.isPSimActivatingNetwork_ ||
         this.isESimUnactivatedProfile_)) {
      this.fireShowDetails_(event);
    } else {
      this.fire('selected', this.item);
      this.focusRequested_ = true;
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_(event) {
    this.fireShowDetails_(event);
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNotAvailableText_() {
    if (!this.networkState || !this.activationUnavailable) {
      return false;
    }

    // If cellular activation is not currently available and |this.networkState|
    // describes an unactivated cellular network, the text should be shown.
    return this.networkState.type === NetworkType.kCellular &&
        this.networkState.typeState.cellular.activationState !==
        ActivationStateType.kActivated;
  },

  /**
   * When the row is focused, this enables aria-live in "polite" mode to notify
   * a11y users when details about the network change or when the list gets
   * re-ordered because of changing signal strengths.
   * @param {boolean} isFocused
   * @return {string}
   * @private
   */
  getLiveStatus_(isFocused) {
    // isFocused is supplied by FocusRowBehavior.
    return this.isFocused ? 'polite' : 'off';
  },

  /**
   * @param {!Event} event
   * @private
   */
  onInstallButtonClick_(event) {
    if (this.disabled_) {
      return;
    }
    this.fire('install-profile', {iccid: this.item.customData.iccid});
    // Stop click from propagating to 'onSelected_()' and firing event twice.
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsESimPendingProfile_() {
    return !!this.item && this.item.hasOwnProperty('customItemType') &&
        this.item.customItemType ===
        NetworkList.CustomItemType.ESIM_PENDING_PROFILE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsESimInstallingProfile_() {
    return !!this.item && this.item.hasOwnProperty('customItemType') &&
        this.item.customItemType ===
        NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsESimUnactivatedProfile_() {
    if (!this.isESimNetwork_()) {
      return false;
    }
    return this.networkState.typeState.cellular.activationState ===
        ActivationStateType.kNotActivated;
  },

  /**
   * @return {boolean}
   * @private
   */
  isUnactivatedPSimNetwork_() {
    if (!this.isPSimNetwork_()) {
      return false;
    }
    return this.networkState.typeState.cellular.activationState ===
        ActivationStateType.kNotActivated;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasPaymentPortalInfo_() {
    if (!this.networkState || !this.networkState.typeState.cellular) {
      return false;
    }
    return !!this.networkState.typeState.cellular.paymentPortal &&
        !!this.networkState.typeState.cellular.paymentPortal.url;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsPSimPendingActivationNetwork_() {
    return this.isUnactivatedPSimNetwork_() && this.hasPaymentPortalInfo_();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowActivateButton_() {
    if (!this.showButtons) {
      return false;
    }
    if (!this.isUserLoggedIn_) {
      return false;
    }

    return this.isPSimPendingActivationNetwork_;
  },

  /**
   * @return {string}
   * @private
   */
  getActivateBtnA11yLabel_() {
    return loadTimeData.getStringF(
        'networkListItemActivateA11yLabel', this.getItemName_());
  },

  /**
   * @param {!Event} event
   * @private
   */
  onActivateButtonClick_(event) {
    this.fire(
        'show-cellular-setup', {pageName: CellularSetupPageName.PSIM_FLOW_UI});
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsPSimUnavailableNetwork_() {
    return this.isUnactivatedPSimNetwork_() && !this.hasPaymentPortalInfo_();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsPSimActivatingNetwork_() {
    if (!this.isPSimNetwork_()) {
      return false;
    }
    return this.networkState.typeState.cellular.activationState ===
        ActivationStateType.kActivating;
  },

  /**
   * @return {boolean}
   * @private
   */
  isBlockedWifiNetwork_() {
    if (!this.item) {
      return false;
    }

    if (this.item.type !== NetworkType.kWiFi) {
      return false;
    }

    if (!this.globalPolicy || this.isPolicySource(this.item.source)) {
      return false;
    }

    if (this.globalPolicy.allowOnlyPolicyWifiNetworksToConnect) {
      return true;
    }

    if (!!this.globalPolicy.allowOnlyPolicyWifiNetworksToConnectIfAvailable &&
        !!this.deviceState && !!this.deviceState.managedNetworkAvailable) {
      return true;
    }

    return !!this.globalPolicy.blockedHexSsids &&
        this.globalPolicy.blockedHexSsids.includes(
            this.item.typeState.wifi.hexSsid);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsBlockedNetwork_() {
    if (!this.item) {
      return false;
    }

    // Only Cellular and WiFi networks can be blocked by administrators.
    if (this.item.type !== NetworkType.kCellular &&
        this.item.type !== NetworkType.kWiFi) {
      return false;
    }

    if (!this.globalPolicy || this.isPolicySource(this.item.source)) {
      return false;
    }

    if (this.item.type === NetworkType.kCellular) {
      return !!this.globalPolicy.allowOnlyPolicyCellularNetworks;
    }

    return this.isBlockedWifiNetwork_();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPolicyIcon_() {
    if (this.isBuiltInVpn_() && this.isBuiltInVpnManagementBlocked) {
      return true;
    }

    return !!this.networkState && this.isPolicySource(this.networkState.source);
  },

  /**
   * @return {!CrPolicyIndicatorType}
   */
  getPolicyIcon_() {
    if (this.isBuiltInVpn_() && this.isBuiltInVpnManagementBlocked) {
      return CrPolicyIndicatorType.USER_POLICY;
    }

    return this.getIndicatorTypeForSource(this.networkState.source);
  },

  /**
   * @return {boolean}
   * @private
   */
  isCellularNetworkScanning_() {
    if (!this.deviceState || !this.deviceState.scanning) {
      return false;
    }

    const iccid = this.networkState && this.networkState.typeState.cellular &&
        this.networkState.typeState.cellular.iccid;
    if (!iccid) {
      return false;
    }

    // Scanning state should be shown only for the active SIM.
    return this.deviceState.simInfos.some(simInfo => {
      return simInfo.iccid === iccid && simInfo.isPrimary;
    });
  },

  /** @private */
  onUnlockButtonClick_() {
    this.isCellularUnlockDialogOpen_ = true;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowUnlockButton_() {
    if (!this.showButtons) {
      return false;
    }
    if (!this.networkState || !this.networkState.typeState.cellular) {
      return false;
    }
    if (this.networkState.typeState.cellular.simLocked &&
        this.networkState.typeState.cellular.simLockType === 'network-pin') {
      return false;
    }
    return this.networkState.typeState.cellular.simLocked;
  },

  /**
   * @return {string}
   * @private
   */
  getUnlockBtnA11yLabel_() {
    return loadTimeData.getStringF(
        'networkListItemUnlockA11YLabel', this.getItemName_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowInstallButton_() {
    if (!this.showButtons) {
      return false;
    }
    return this.isESimPendingProfile_;
  },

  /**
   * @return {string}
   * @private
   */
  getInstallBtnA11yLabel_() {
    return loadTimeData.getStringF(
        'networkListItemDownloadA11yLabel', this.getItemName_());
  },

  /**
   * @param {boolean} newValue
   * @param {boolean|undefined} oldValue
   * @private
   */
  disabledChanged_(newValue, oldValue) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled_) {
      this.blur();
    }
    this.setAttribute('aria-disabled', !!this.disabled_);
  },

  /**
   * @param {!PortalState} portalState
   * @return {boolean}
   * @private
   */
  isPortalState_(portalState) {
    return portalState === PortalState.kPortal ||
        portalState === PortalState.kPortalSuspected;
  },

  /**
   * Return true if current network is pSIM, requires activation and user is
   * not logged in or gone through device setup(OOBE).
   * @return {boolean}
   * @private
   */
  isPsimPendingActivationWhileLoggedOut_() {
    return !this.isUserLoggedIn_ && this.isPSimPendingActivationNetwork_;
  },
});

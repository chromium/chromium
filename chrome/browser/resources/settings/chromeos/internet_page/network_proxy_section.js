// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element hosting <network-proxy> in the internet
 * detail page. This element is responsible for setting 'Allow proxies for
 * shared networks'.
 */

Polymer({
  is: 'network-proxy-section',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: Object,

    disabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Reflects prefs.settings.use_shared_proxies for data binding.
     * @private
     */
    useSharedProxies_: Boolean,
  },

  observers: [
    'useSharedProxiesChanged_(prefs.settings.use_shared_proxies.value)',
  ],

  /**
   * Returns the allow shared CrToggleElement.
   * @return {?CrToggleElement}
   */
  getAllowSharedToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#allowShared'));
  },

  /** @protected settings.RouteObserverBehavior */
  currentRouteChanged(newRoute) {
    if (newRoute === settings.routes.NETWORK_DETAIL) {
      /** @type {NetworkProxyElement} */ (this.$$('network-proxy')).reset();
    }
  },

  /** @private */
  useSharedProxiesChanged_() {
    const pref = this.getPref('settings.use_shared_proxies');
    this.useSharedProxies_ = !!pref && !!pref.value;
  },

  /**
   * @return {boolean}
   * @private
   */
  isShared_() {
    const mojom = chromeos.networkConfig.mojom;
    return this.managedProperties.source === mojom.OncSource.kDevice ||
        this.managedProperties.source === mojom.OncSource.kDevicePolicy;
  },

  /**
   * @return {!chromeos.networkConfig.mojom.ManagedString|undefined}
   * @private
   */
  getProxySettingsTypeProperty_() {
    const proxySettings = this.managedProperties.proxySettings;
    return proxySettings ? proxySettings.type : undefined;
  },

  /**
   * @param {boolean} allowShared
   * @return {string}
   * @private
   */
  getAllowSharedDialogTitle_(allowShared) {
    if (allowShared) {
      return this.i18n('networkProxyAllowSharedDisableWarningTitle');
    }
    return this.i18n('networkProxyAllowSharedEnableWarningTitle');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNetworkPolicyIndicator_() {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && !this.isExtensionControlled(property) &&
        this.isNetworkPolicyEnforced(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowExtensionIndicator_() {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && this.isExtensionControlled(property);
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean}
   * @private
   */
  shouldShowAllowShared_(property) {
    if (!this.isShared_()) {
      return false;
    }
    // We currently do not accurately determine the source if the policy
    // controlling the proxy setting, so always show the 'allow shared'
    // toggle for shared networks. http://crbug.com/662529.
    return true;
  },

  /**
   * Handles the change event for the shared proxy checkbox. Shows a
   * confirmation dialog.
   * @param {!Event} event
   * @private
   */
  onAllowSharedProxiesChange_(event) {
    this.$.confirmAllowSharedDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onAllowSharedDialogConfirm_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .sendPrefChange();
    this.$.confirmAllowSharedDialog.close();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onAllowSharedDialogCancel_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .resetToPrefValue();
    this.$.confirmAllowSharedDialog.close();
  },

  /** @private */
  onAllowSharedDialogClose_() {
    this.$.allowShared.focus();
  },
});

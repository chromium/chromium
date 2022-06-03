// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Wrapper for multidevice-feature-item that allows displaying the Smart Lock
 * feature row outside of the multidevice page. Manages the browser proxy and
 * handles the feature toggle click event. Requires that the hosting page pass
 * in an auth token.
 */
Polymer({
  is: 'settings-multidevice-smartlock-item',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Alias for allowing Polymer bindings to settings.routes.
     * @type {?OsSettingsRoutes}
     */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * Authentication token provided by lock-screen-password-prompt-dialog.
     * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken: {
      type: Object,
    },
  },

  listeners: {
    'feature-toggle-clicked': 'onFeatureToggleClicked_',
  },

  /** @private {?settings.MultiDeviceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.MultiDeviceBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'settings.updateMultidevicePageContentData',
        this.onPageContentDataChanged_.bind(this));

    this.browserProxy_.getPageContentData().then(
        this.onPageContentDataChanged_.bind(this));
  },

  /** @override */
  focus() {
    this.$.smartLockItem.focus();
  },

  /**
   * @param {!settings.MultiDevicePageContentData} newData
   * @private
   */
  onPageContentDataChanged_(newData) {
    this.pageContentData = newData;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowFeature_() {
    // We only show the feature when it is editable, because a disabled toggle
    // is confusing for the user without greater context.
    return this.isFeatureSupported(settings.MultiDeviceFeature.SMART_LOCK) &&
        this.pageContentData.mode ===
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED &&
        this.isFeatureStateEditable(settings.MultiDeviceFeature.SMART_LOCK);
  },

  /**
   * Attempt to enable the provided feature. The authentication token is
   * provided by the parent element.
   * TODO(crbug.com/1229430) refactor to avoid duplicating code from the
   * multidevice page
   *
   * @param {!CustomEvent<!{
   *     feature: !settings.MultiDeviceFeature,
   *     enabled: boolean
   * }>} event
   * @private
   */
  onFeatureToggleClicked_(event) {
    const feature = event.detail.feature;
    const enabled = event.detail.enabled;

    this.browserProxy_.setFeatureEnabledState(
        feature, enabled, this.authToken.token);
    settings.recordSettingChange();
  },
});

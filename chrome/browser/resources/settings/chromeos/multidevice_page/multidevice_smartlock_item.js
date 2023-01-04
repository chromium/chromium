// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Wrapper for multidevice-feature-item that allows displaying the Smart Lock
 * feature row outside of the multidevice page. Manages the browser proxy and
 * handles the feature toggle click event. Requires that the hosting page pass
 * in an auth token.
 */

import './multidevice_feature_item.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {OsSettingsRoutes} from '../os_settings_routes.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDevicePageContentData, MultiDeviceSettingsMode} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsMultideviceSmartlockItemElementBase = mixinBehaviors(
    [MultiDeviceFeatureBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsMultideviceSmartlockItemElement extends
    SettingsMultideviceSmartlockItemElementBase {
  static get is() {
    return 'settings-multidevice-smartlock-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Alias for allowing Polymer bindings to routes.
       * @type {?OsSettingsRoutes}
       */
      routes: {
        type: Object,
        value: routes,
      },

      /**
       * Authentication token provided by lock-screen-password-prompt-dialog.
       * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
       */
      authToken: {
        type: Object,
      },
    };
  }

  constructor() {
    super();

    /** @private {!MultiDeviceBrowserProxy} */
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('feature-toggle-clicked', (event) => {
      this.onFeatureToggleClicked_(
          /**
           * @type {!CustomEvent<!{feature: !MultiDeviceFeature, enabled:
           *  boolean}>}
           */
          (event));
    });

    this.addWebUIListener(
        'settings.updateMultidevicePageContentData',
        this.onPageContentDataChanged_.bind(this));

    this.browserProxy_.getPageContentData().then(
        this.onPageContentDataChanged_.bind(this));
  }

  /** @override */
  focus() {
    this.$.smartLockItem.focus();
  }

  /**
   * @param {!MultiDevicePageContentData} newData
   * @private
   */
  onPageContentDataChanged_(newData) {
    this.pageContentData = newData;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowFeature_() {
    // We only show the feature when it is editable, because a disabled toggle
    // is confusing for the user without greater context.
    return this.isFeatureSupported(MultiDeviceFeature.SMART_LOCK) &&
        this.pageContentData.mode ===
        MultiDeviceSettingsMode.HOST_SET_VERIFIED &&
        this.isFeatureStateEditable(MultiDeviceFeature.SMART_LOCK);
  }

  /**
   * Attempt to enable the provided feature. The authentication token is
   * provided by the parent element.
   * TODO(crbug.com/1229430) refactor to avoid duplicating code from the
   * multidevice page
   *
   * @param {!CustomEvent<!{
   *     feature: !MultiDeviceFeature,
   *     enabled: boolean
   * }>} event
   * @private
   */
  onFeatureToggleClicked_(event) {
    const feature = event.detail.feature;
    const enabled = event.detail.enabled;

    this.browserProxy_.setFeatureEnabledState(
        feature, enabled, this.authToken.token);
    recordSettingChange();
  }
}

customElements.define(
    SettingsMultideviceSmartlockItemElement.is,
    SettingsMultideviceSmartlockItemElement);

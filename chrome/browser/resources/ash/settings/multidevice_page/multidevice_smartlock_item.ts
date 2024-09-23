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

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDevicePageContentData, MultiDeviceSettingsMode} from './multidevice_constants.js';
import {SettingsMultideviceFeatureItemElement} from './multidevice_feature_item.js';
import {MultiDeviceFeatureMixin} from './multidevice_feature_mixin.js';
import {getTemplate} from './multidevice_smartlock_item.html.js';

export interface SettingsMultideviceSmartlockItemElement {
  $: {
    smartLockItem: SettingsMultideviceFeatureItemElement,
  };
}

const SettingsMultideviceSmartlockItemElementBase =
    MultiDeviceFeatureMixin(WebUiListenerMixin(PolymerElement));

export class SettingsMultideviceSmartlockItemElement extends
    SettingsMultideviceSmartlockItemElementBase {
  static get is() {
    return 'settings-multidevice-smartlock-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Authentication token provided by lock-screen-password-prompt-dialog.
       */
      authToken: {
        type: Object,
      },
    };
  }

  authToken: chrome.quickUnlockPrivate.TokenInfo|undefined;
  private browserProxy_: MultiDeviceBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('feature-toggle-clicked', (event) => {
      this.onFeatureToggleClicked_(event);
    });

    this.addWebUiListener(
        'settings.updateMultidevicePageContentData',
        this.onPageContentDataChanged_.bind(this));

    this.browserProxy_.getPageContentData().then(
        this.onPageContentDataChanged_.bind(this));
  }

  override focus(): void {
    this.$.smartLockItem.focus();
  }

  private onPageContentDataChanged_(newData: MultiDevicePageContentData): void {
    this.pageContentData = newData;
  }

  private shouldShowFeature_(): boolean {
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
   */
  private onFeatureToggleClicked_(
      event: CustomEvent<{feature: MultiDeviceFeature, enabled: boolean}>):
      void {
    const feature = event.detail.feature;
    const enabled = event.detail.enabled;

    this.browserProxy_.setFeatureEnabledState(
        feature, enabled, this.authToken!.token);
    recordSettingChange(Setting.kSmartLockOnOff);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceSmartlockItemElement.is]:
        SettingsMultideviceSmartlockItemElement;
  }
}

customElements.define(
    SettingsMultideviceSmartlockItemElement.is,
    SettingsMultideviceSmartlockItemElement);

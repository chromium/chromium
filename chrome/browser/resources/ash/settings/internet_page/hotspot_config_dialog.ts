// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to set hotspot configuration
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './internet_shared.css.js';

import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotConfig, HotspotInfo, SetHotspotConfigResult, WiFiBand, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './hotspot_config_dialog.html.js';

export enum WiFiSecurityType {
  WPA2 = 'WPA2',
  WPA3 = 'WPA3',
  WPA2WPA3 = 'WPA2WPA3',
}

const MIN_WIFI_PASSWORD_LENGTH = 8;
const MAX_WIFI_PASSWORD_LENGTH = 63;
const MAX_HOTSPOT_SSID_LENGTH = 32;

export interface HotspotConfigDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const HotspotConfigDialogElementBase = I18nMixin(PolymerElement);

export class HotspotConfigDialogElement extends HotspotConfigDialogElementBase {
  static get is() {
    return 'hotspot-config-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hotspotInfo: {
        type: Object,
      },

      hotspotSsid_: {
        type: String,
        value: '',
        observer: 'onSsidChanged_',
      },

      isSsidInvalid_: {
        type: Boolean,
        value: false,
      },

      hotspotPassword_: {
        type: String,
        value: '',
        observer: 'onPasswordChanged_',
      },

      isPasswordInvalid_: {
        type: Boolean,
        value: false,
      },

      securityType_: {
        type: String,
        value: '',
      },

      isRandomizeBssidToggleOn_: {
        type: Boolean,
        value: true,
      },

      isExtendCompatibilityToggleOn_: {
        type: Boolean,
        value: false,
      },

      error_: {
        type: String,
        value: '',
      },
    };
  }

  hotspotInfo: HotspotInfo;
  private hotspotSsid_: string;
  private isSsidInvalid_: boolean;
  private hotspotPassword_: string;
  private isPasswordInvalid_: boolean;
  private securityType_: string;
  private isRandomizeBssidToggleOn_: boolean;
  private isExtendCompatibilityToggleOn_: boolean;
  private error_: string;

  override connectedCallback(): void {
    super.connectedCallback();

    this.init_();
  }

  private init_(): void {
    this.hotspotSsid_ = castExists(this.hotspotInfo.config!.ssid);
    this.hotspotPassword_ = castExists(this.hotspotInfo.config!.passphrase);
    this.isRandomizeBssidToggleOn_ =
        castExists(this.hotspotInfo.config!.bssidRandomization);
    this.isExtendCompatibilityToggleOn_ =
        this.hotspotInfo.config!.band === WiFiBand.k2_4GHz;
    this.securityType_ = this.getWifiSecurityTypeString_(
        castExists(this.hotspotInfo.config!.security));
  }

  private onSsidChanged_(): void {
    this.isSsidInvalid_ = this.hotspotSsid_.length === 0 ||
        this.hotspotSsid_.length > MAX_HOTSPOT_SSID_LENGTH;
  }

  private onPasswordChanged_(): void {
    this.isPasswordInvalid_ =
        this.hotspotPassword_.length < MIN_WIFI_PASSWORD_LENGTH ||
        this.hotspotPassword_.length > MAX_WIFI_PASSWORD_LENGTH;
  }

  private getWifiSecurityTypeString_(security: WiFiSecurityMode): string {
    if (security === WiFiSecurityMode.kWpa2) {
      return WiFiSecurityType.WPA2;
    }
    if (security === WiFiSecurityMode.kWpa3) {
      return WiFiSecurityType.WPA3;
    }
    if (security === WiFiSecurityMode.kWpa2Wpa3) {
      return WiFiSecurityType.WPA2WPA3;
    }
    assertNotReached();
  }

  private getSecurityModeFromString_(security: string): WiFiSecurityMode {
    if (security === WiFiSecurityType.WPA2) {
      return WiFiSecurityMode.kWpa2;
    }
    if (security === WiFiSecurityType.WPA3) {
      return WiFiSecurityMode.kWpa3;
    }
    if (security === WiFiSecurityType.WPA2WPA3) {
      return WiFiSecurityMode.kWpa2Wpa3;
    }
    assertNotReached();
  }

  private getSecurityItems_(): string[] {
    return this.hotspotInfo!.allowedWifiSecurityModes.map(security => {
      return this.getWifiSecurityTypeString_(security);
    });
  }

  private getSsidInputInfoClass_(): string {
    if (!this.isSsidInvalid_) {
      return 'input-info';
    }
    return 'input-info error';
  }

  private getSsidInputInfo_(): string {
    if (this.hotspotSsid_.length === 0) {
      return this.i18n('hotspotConfigNameEmptyInfo');
    }
    if (this.hotspotSsid_.length > MAX_HOTSPOT_SSID_LENGTH) {
      return this.i18n('hotspotConfigNameTooLongInfo');
    }
    return this.i18n('hotspotConfigNameInfo');
  }

  private getPasswordInputInfoClass_(): string {
    if (!this.isPasswordInvalid_) {
      return 'input-info';
    }
    return 'input-info error';
  }

  private isSaveButtonDisabled_(): boolean {
    return this.isSsidInvalid_ || this.isPasswordInvalid_;
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private async onSaveClick_(): Promise<void> {
    const configToSet: HotspotConfig = {
      ssid: this.hotspotSsid_,
      passphrase: this.hotspotPassword_,
      security: this.getSecurityModeFromString_(this.securityType_),
      band: this.isExtendCompatibilityToggleOn_ ? WiFiBand.k2_4GHz :
                                                  WiFiBand.kAutoChoose,
      bssidRandomization: this.isRandomizeBssidToggleOn_,
      autoDisable: castExists(this.hotspotInfo.config!.autoDisable),
    };
    const response = await getHotspotConfig().setHotspotConfig(configToSet);
    if (response.result === SetHotspotConfigResult.kSuccess) {
      this.$.dialog.close();
      return;
    }
    if (response.result ===
        SetHotspotConfigResult.kFailedInvalidConfiguration) {
      this.error_ = this.i18n('hotspotConfigInvalidConfigurationErrorMessage');
    } else if (response.result === SetHotspotConfigResult.kFailedNotLogin) {
      this.error_ = this.i18n('hotspotConfigNotLoginErrorMessage');
    } else {
      this.error_ = this.i18n('hotspotConfigGeneralErrorMessage');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HotspotConfigDialogElement.is]: HotspotConfigDialogElement;
  }
}

customElements.define(
    HotspotConfigDialogElement.is, HotspotConfigDialogElement);

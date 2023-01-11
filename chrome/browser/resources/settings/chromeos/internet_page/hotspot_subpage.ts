// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing and configuring Hotspot.
 */

import '../../settings_shared.css.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';

import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState, SetHotspotConfigResult} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './hotspot_subpage.html.js';

const SettingsHotspotSubpageElementBase = I18nMixin(PolymerElement);

class SettingsHotspotSubpageElement extends SettingsHotspotSubpageElementBase {
  static get is() {
    return 'settings-hotspot-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hotspotInfo: {
        type: Object,
        observer: 'onHotspotInfoChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |HotspotInfo| state changes or when the user presses the toggle.
       */
      isHotspotToggleOn_: {
        type: Boolean,
        observer: 'onHotspotToggleChanged_',
      },

      /**
       * Hotspot auto disabled state.
       */
      autoDisableVirtualPref_: {
        type: Object,
        value() {
          return {
            key: 'fakeAutoDisablePref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },
    };
  }

  hotspotInfo: HotspotInfo|undefined;
  private isHotspotToggleOn_: boolean;
  private autoDisableVirtualPref_: chrome.settingsPrivate.PrefObject<boolean>;

  private onHotspotInfoChanged_(
      newValue: HotspotInfo, _oldValue: HotspotInfo|undefined): void {
    this.isHotspotToggleOn_ = newValue.state === HotspotState.kEnabled ||
        newValue.state === HotspotState.kEnabling;
    this.updateAutoDisablePref_();
  }

  /**
   * Observer for isHotspotToggleOn_ that returns early until the previous
   * value was not undefined to avoid wrongly toggling the HotspotInfo state.
   */
  private onHotspotToggleChanged_(
      newValue: boolean, oldValue: boolean|undefined): void {
    if (oldValue === undefined) {
      return;
    }
    // If the toggle value changed but the toggle is disabled, the change came
    // from CrosHotspotConfig, not the user. Don't attempt to turn the hotspot
    // on or off.
    if (this.isToggleDisabled_()) {
      return;
    }

    this.setHotspotEnabledState_(newValue);
  }

  private updateAutoDisablePref_(): void {
    if (!this.hotspotInfo?.config) {
      return;
    }
    const newPrefValue: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'fakeAutoDisablePref',
      value: this.hotspotInfo.config.autoDisable,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    this.autoDisableVirtualPref_ = newPrefValue;
  }

  private isToggleDisabled_(): boolean {
    if (!this.hotspotInfo) {
      return true;
    }
    if (this.hotspotInfo.allowStatus !== HotspotAllowStatus.kAllowed) {
      return true;
    }
    return this.hotspotInfo.state === HotspotState.kEnabling ||
        this.hotspotInfo.state === HotspotState.kDisabling;
  }

  private getOnOffString_(isHotspotToggleOn: boolean): string {
    return isHotspotToggleOn ? this.i18n('hotspotSummaryStateOn') :
                               this.i18n('hotspotSummaryStateOff');
  }

  private setHotspotEnabledState_(enabled: boolean): void {
    if (enabled) {
      getHotspotConfig().enableHotspot();
      return;
    }
    getHotspotConfig().disableHotspot();
  }

  private announceHotspotToggleChange_(): void {
    getAnnouncerInstance().announce(
        this.isHotspotToggleOn_ ? this.i18n('hotspotEnabledA11yLabel') :
                                  this.i18n('hotspotDisabledA11yLabel'));
  }

  private getHotspotConfigSsid_(ssid: string|undefined): string {
    return ssid || '';
  }

  private getHotspotConnectedDeviceCount_(clientCount: number|
                                          undefined): number {
    return clientCount || 0;
  }

  private showHotspotAutoDisableToggle_(hotspotInfo: HotspotInfo|
                                        undefined): boolean {
    return !!hotspotInfo?.config;
  }

  private onHotspotConfigureTap_() {
    const event = new CustomEvent('show-hotspot-config-dialog', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(event);
  }

  private async onAutoDisableChange_(): Promise<void> {
    const configToSet = castExists(this.hotspotInfo!.config);
    configToSet.autoDisable = this.autoDisableVirtualPref_.value;
    const response = await getHotspotConfig().setHotspotConfig(configToSet);
    if (response.result !== SetHotspotConfigResult.kSuccess) {
      // Flip back the toggle if not set successfully.
      const newPrefValue: chrome.settingsPrivate.PrefObject<boolean> = {
        key: 'fakeEnabledPref',
        value: !configToSet.autoDisable,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
      };
      this.autoDisableVirtualPref_ = newPrefValue;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsHotspotSubpageElement.is]: SettingsHotspotSubpageElement;
  }
}

customElements.define(
    SettingsHotspotSubpageElement.is, SettingsHotspotSubpageElement);

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing and configuring Hotspot.
 */

import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState, SetHotspotConfigResult} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './hotspot_subpage.html.js';

const SettingsHotspotSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsHotspotSubpageElement extends
    SettingsHotspotSubpageElementBase {
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
        value: false,
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

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>(
            [Setting.kHotspotOnOff, Setting.kHotspotAutoDisabled]),
      },
    };
  }

  hotspotInfo: HotspotInfo|undefined;
  private isHotspotToggleOn_: boolean;
  private autoDisableVirtualPref_: chrome.settingsPrivate.PrefObject<boolean>;

  override currentRouteChanged(route: Route, _oldRoute?: Route): void {
    // Does not apply to this page.
    if (route !== routes.HOTSPOT_DETAIL) {
      return;
    }

    this.attemptDeepLink();
  }

  private onHotspotInfoChanged_(
      newValue: HotspotInfo, _oldValue: HotspotInfo|undefined): void {
    this.isHotspotToggleOn_ = newValue.state === HotspotState.kEnabled ||
        newValue.state === HotspotState.kEnabling;
    this.updateAutoDisablePref_();
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
    if (this.hotspotInfo.state === HotspotState.kDisabling) {
      return true;
    }
    if (this.hotspotInfo.state === HotspotState.kEnabling ||
        this.hotspotInfo.state === HotspotState.kEnabled) {
      return false;
    }
    return this.hotspotInfo.allowStatus !== HotspotAllowStatus.kAllowed;
  }

  private getOnOffString_(): string {
    if (!this.hotspotInfo) {
      return this.i18n('hotspotSummaryStateOff');
    }
    if (this.hotspotInfo.state === HotspotState.kEnabling) {
      return this.i18n('hotspotSummaryStateTurningOn');
    }
    if (this.hotspotInfo.state === HotspotState.kEnabled) {
      return this.i18n('hotspotSummaryStateOn');
    }
    if (this.hotspotInfo.state === HotspotState.kDisabling) {
      return this.i18n('hotspotSummaryStateTurningOff');
    }

    return this.i18n('hotspotSummaryStateOff');
  }

  private setHotspotEnabledState_(enabled: boolean): void {
    if (enabled) {
      getHotspotConfig().enableHotspot();
      return;
    }
    getHotspotConfig().disableHotspot();
  }

  private onHotspotToggleChange_(): void {
    this.setHotspotEnabledState_(this.isHotspotToggleOn_);
    getAnnouncerInstance().announce(
        this.isHotspotToggleOn_ ? this.i18n('hotspotEnabledA11yLabel') :
                                  this.i18n('hotspotDisabledA11yLabel'));
  }

  private getHotspotConfigSsid_(ssid: string|undefined): string {
    return ssid || '';
  }

  private hideConnectedDeviceCount_(): boolean {
    return this.hotspotInfo?.state !== HotspotState.kEnabled &&
        this.hotspotInfo?.state !== HotspotState.kDisabling;
  }

  private getHotspotConnectedDeviceCount_(clientCount: number|
                                          undefined): number {
    return clientCount || 0;
  }

  private showHotspotAutoDisableToggle_(hotspotInfo: HotspotInfo|
                                        undefined): boolean {
    return !!hotspotInfo?.config;
  }

  private onHotspotConfigureClick_(): void {
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

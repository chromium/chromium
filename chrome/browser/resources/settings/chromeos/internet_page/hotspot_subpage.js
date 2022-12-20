// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing and configuring Hotspot.
 */

import '../../settings_shared.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {HotspotAllowStatus, HotspotConfig, HotspotControlResult, HotspotInfo, HotspotState} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsHotspotSubpageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsHotspotSubpageElement extends SettingsHotspotSubpageElementBase {
  static get is() {
    return 'settings-hotspot-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!HotspotInfo|undefined} */
      hotspotInfo: {
        type: Object,
        observer: 'onHotspotInfoChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |HotspotInfo| state changes or when the user presses the toggle.
       * @private
       */
      isHotspotToggleOn_: {
        type: Boolean,
        observer: 'onHotspotToggleChanged_',
      },
    };
  }

  /** @private */
  onHotspotInfoChanged_() {
    assert(this.hotspotInfo);
    this.isHotspotToggleOn_ =
        this.hotspotInfo.state === HotspotState.kEnabled ||
        this.hotspotInfo.state === HotspotState.kEnabling;
  }

  /**
   * Observer for isHotspotToggleOn_ that returns early until the previous
   * value was not undefined to avoid wrongly toggling the HotspotInfo state.
   * @param {boolean} newValue
   * @param {boolean|undefined} oldValue
   * @private
   */
  onHotspotToggleChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      return;
    }
    // If the toggle value changed but the toggle is disabled, the change came
    // from CrosHotspotConfig, not the user. Don't attempt to turn the hotspot
    // on or off.
    if (this.isToggleDisabled_()) {
      return;
    }

    this.setHotspotEnabledState_(this.isHotspotToggleOn_);
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    if (!this.hotspotInfo) {
      return true;
    }
    if (this.hotspotInfo.allowStatus !== HotspotAllowStatus.kAllowed) {
      return true;
    }
    return this.hotspotInfo.state === HotspotState.kEnabling ||
        this.hotspotInfo.state === HotspotState.kDisabling;
  }

  /**
   * @return {string}
   * @private
   */
  getOnOffString_() {
    return this.isHotspotToggleOn_ ? this.i18n('hotspotSummaryStateOn') :
                                     this.i18n('hotspotSummaryStateOff');
  }

  /**
   * Enables or disables hotspot.
   * @param {boolean} enabled
   * @private
   */
  setHotspotEnabledState_(enabled) {
    if (enabled) {
      getHotspotConfig().enableHotspot();
      return;
    }
    getHotspotConfig().disableHotspot();
  }

  /** @private */
  announceHotspotToggleChange_() {
    getAnnouncerInstance().announce(
        this.isHotspotToggleOn_ ? this.i18n('hotspotEnabledA11yLabel') :
                                  this.i18n('hotspotDisabledA11yLabel'));
  }

  /**
   * @return {string}
   * @private
   */
  getHotspotConfigSSID_() {
    return this.hotspotInfo?.config?.ssid || '';
  }

  /**
   * @return {number}
   * @private
   */
  getHotspotConnectedDeviceCount_() {
    return this.hotspotInfo?.clientCount || 0;
  }
}

customElements.define(
    SettingsHotspotSubpageElement.is, SettingsHotspotSubpageElement);

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a hotspot summary item row with
 * a toggle button below the network summary item.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, CrosHotspotConfigObserverReceiver, HotspotAllowStatus, HotspotControlResult, HotspotInfo, HotspotState} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {Router} from '../router.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const HotspotSummaryItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class HotspotSummaryItemElement extends HotspotSummaryItemElementBase {
  static get is() {
    return 'hotspot-summary-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @public {!HotspotInfo|undefined} */
      hotspotInfo: {
        type: Object,
        observer: 'onHotspotInfoChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |HotspotInfo| state changes or when the user presses the
       * toggle.
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
    this.isHotspotToggleOn_ =
        this.hotspotInfo.state === HotspotState.kEnabled ||
        this.hotspotInfo.state === HotspotState.kEnabling;
  }


  /** @private */
  navigateToDetailPage_() {
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
  }

  /** @private */
  getSecondaryLabel_() {
    return this.isHotspotToggleOn_ ? this.i18n('hotspotSummaryStateOn') :
                                     this.i18n('hotspotSummaryStateOff');
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

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    if (!this.hotspotInfo) {
      return true;
    }
    if (this.hotspotInfo.allowStatus ===
        HotspotAllowStatus.kDisallowedByPolicy) {
      return true;
    }
    return this.hotspotInfo.state === HotspotState.kEnabling ||
        this.hotspotInfo.state === HotspotState.kDisabling;
  }

  /**
   * @return {string}
   * @private
   */
  getIconClass_() {
    if (!this.isHotspotToggleOn_) {
      return 'os-settings:hotspot-disabled';
    }

    return 'os-settings:hotspot-enabled';
  }

  /** @private */
  announceHotspotToggleChange_() {
    getAnnouncerInstance().announce(
        this.isHotspotToggleOn_ ? this.i18n('hotspotEnabledA11yLabel') :
                                  this.i18n('hotspotDisabledA11yLabel'));
  }
}

customElements.define(HotspotSummaryItemElement.is, HotspotSummaryItemElement);

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
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {Router} from '../router.js';

import {getTemplate} from './hotspot_summary_item.html.js';

const HotspotSummaryItemElementBase = I18nMixin(PolymerElement);

class HotspotSummaryItemElement extends HotspotSummaryItemElementBase {
  static get is() {
    return 'hotspot-summary-item' as const;
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
       * the |HotspotInfo| state changes or when the user presses the
       * toggle.
       */
      isHotspotToggleOn_: {
        type: Boolean,
        observer: 'onHotspotToggleChanged_',
      },
    };
  }

  hotspotInfo: HotspotInfo|undefined;
  private isHotspotToggleOn_: boolean;

  private onHotspotInfoChanged_(
      newValue: HotspotInfo, _oldValue: HotspotInfo|undefined): void {
    this.isHotspotToggleOn_ = newValue.state === HotspotState.kEnabled ||
        newValue.state === HotspotState.kEnabling;
  }


  private navigateToDetailPage_(): void {
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
  }

  private getSecondaryLabel_(isHotspotToggleOn: boolean): string {
    return isHotspotToggleOn ? this.i18n('hotspotSummaryStateOn') :
                               this.i18n('hotspotSummaryStateOff');
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

  private setHotspotEnabledState_(enabled: boolean): void {
    if (enabled) {
      getHotspotConfig().enableHotspot();
      return;
    }
    getHotspotConfig().disableHotspot();
  }

  private isToggleDisabled_(): boolean {
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

  private getIconClass_(isHotspotToggleOn: boolean): string {
    if (isHotspotToggleOn) {
      return 'os-settings:hotspot-enabled';
    }
    return 'os-settings:hotspot-disabled';
  }

  private announceHotspotToggleChange_(): void {
    getAnnouncerInstance().announce(
        this.isHotspotToggleOn_ ? this.i18n('hotspotEnabledA11yLabel') :
                                  this.i18n('hotspotDisabledA11yLabel'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HotspotSummaryItemElement.is]: HotspotSummaryItemElement;
  }
}

customElements.define(HotspotSummaryItemElement.is, HotspotSummaryItemElement);

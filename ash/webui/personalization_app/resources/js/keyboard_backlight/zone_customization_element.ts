// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The zone customization dialog that allows users to customize the rgb keyboard
 * zone color.
 */

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './color_icon_element.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {BacklightColor, CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {RAINBOW, staticColorIds} from '../utils.js';

import {setBacklightZoneColor} from './keyboard_backlight_controller.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';
import {getTemplate} from './zone_customization_element.html.js';

export interface ZoneCustomizationElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class ZoneCustomizationElement extends WithPersonalizationStore {
  static get is() {
    return 'zone-customization';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The currently selected zone index. */
      ironSelectedZone_: Object,

      /** The current backlight state in the system. */
      currentBacklightState_: Object,

      /** The current backlight zone colors. */
      zoneColors_: {
        type: Array,
        computed: 'computeZoneColors_(currentBacklightState_, zoneCount_)',
      },

      /** Number of zones available for customization */
      zoneCount_: {
        type: Number,
        value() {
          return loadTimeData.getInteger('keyboardBacklightZoneCount');
        },
      },

      /** The zone indexes (of zoneColors_) to indicate the zone number. */
      zoneIdxs_: {
        type: Array,
        computed: 'computeZoneIdxs_(zoneCount_)',
      },
    };
  }

  private ironSelectedZone_: number;
  private currentBacklightState_: CurrentBacklightState|null;
  private zoneColors_: BacklightColor[]|null;
  private zoneCount_: number;
  private zoneIds_: number[];

  override connectedCallback() {
    super.connectedCallback();
    this.watch(
        'currentBacklightState_',
        state => state.keyboardBacklight.currentBacklightState);
    this.updateFromStore();
  }

  showModal() {
    this.$.dialog.showModal();
  }

  /**
   * Sets zone one color to red. TODO(b/265855838): Remove after the color
   * selector is implemented.
   */
  private setZoneOneToRed_() {
    setBacklightZoneColor(
        0, BacklightColor.kRed, getKeyboardBacklightProvider());
  }

  private computeZoneIdxs_(): number[] {
    return [...Array(this.zoneCount_).keys()];
  }

  private computeZoneColors_(): BacklightColor[]|null {
    if (this.currentBacklightState_ && this.currentBacklightState_.zoneColors) {
      return this.currentBacklightState_.zoneColors;
    } else if (
        this.currentBacklightState_ &&
        this.currentBacklightState_.color !== undefined) {
      return Array(this.zoneCount_).fill(this.currentBacklightState_.color);
    }
    return null;
  }

  private getTabIndex_(zoneIdx: number): string {
    return zoneIdx === 0 ? '0' : '-1';
  }

  private getZoneTitle_(zoneIdx: number): string {
    return loadTimeData.getStringF('zoneTitle', zoneIdx + 1);
  }

  // Returns the matching colorId for each zone based on its zone color.
  private getColorId_(zoneIdx: number, zoneColors: BacklightColor[]): string
      |null {
    if (!zoneColors) {
      return null;
    }
    const zoneColor = zoneColors[zoneIdx];
    if (zoneColor === BacklightColor.kRainbow) {
      return RAINBOW;
    }
    // BacklightColor value matches with the index of staticColorIds.
    // Ex: zoneColor value is BacklightColor.kGreen or 4, corresponding to
    // staticColorIds[4] which is GREEN.
    return staticColorIds[zoneColor];
  }

  private closeZoneCustomizationDialog_() {
    this.$.dialog.close();
  }
}

customElements.define(ZoneCustomizationElement.is, ZoneCustomizationElement);

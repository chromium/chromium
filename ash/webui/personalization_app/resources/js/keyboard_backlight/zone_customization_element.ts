// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The zone customization dialog that allows users to customize the rgb keyboard
 * zone color.
 */

import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './color_icon_element.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {BacklightColor, CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getPresetColors, isSelectionEvent, RAINBOW, staticColorIds} from '../utils.js';

import {PresetColorSelectedEvent} from './color_selector_element.js';
import {setBacklightZoneColor} from './keyboard_backlight_controller.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';
import {getTemplate} from './zone_customization_element.html.js';

export interface ZoneCustomizationElement {
  $: {
    dialog: CrDialogElement,
    zoneKeys: IronA11yKeysElement,
    zoneSelector: IronSelectorElement,
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
      zoneSelected_: {
        type: Number,
        value: 0,
      },

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

  private zoneSelected_: number;
  private ironSelectedZone_: HTMLElement;
  private currentBacklightState_: CurrentBacklightState|null;
  private zoneColors_: BacklightColor[]|null;
  private zoneCount_: number;
  private zoneIdxs_: number[];

  override ready() {
    super.ready();
    this.$.zoneKeys.target = this.$.zoneSelector;
  }

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

  /** Handle keyboard navigation. */
  private onZoneKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.zoneSelector;
    const prevButton = this.ironSelectedZone_;

    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.ironSelectedZone_) {
      this.ironSelectedZone_.setAttribute('tabindex', '0');
      this.ironSelectedZone_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  private onClickZoneButton_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    const eventTarget = event.currentTarget as HTMLElement;
    this.zoneSelected_ = Number(eventTarget.dataset['zoneIdx']);
  }

  private onWallpaperColorSelected_() {
    if (!this.zoneColors_) {
      return;
    }
    const currentColor =
        this.getSelectedColor_(this.zoneSelected_, this.zoneColors_);
    if (currentColor !== BacklightColor.kWallpaper) {
      setBacklightZoneColor(
          this.zoneSelected_, BacklightColor.kWallpaper, this.zoneColors_,
          getKeyboardBacklightProvider(), this.getStore());
    }
  }

  private onPresetColorSelected_(e: PresetColorSelectedEvent) {
    if (!this.zoneColors_) {
      return;
    }
    const currentColor =
        this.getSelectedColor_(this.zoneSelected_, this.zoneColors_);
    const colorId = e.detail.colorId;
    assert(colorId !== undefined, 'colorId not found');
    const newColor = getPresetColors()[colorId].enumVal;
    if (currentColor !== newColor) {
      setBacklightZoneColor(
          this.zoneSelected_, newColor, this.zoneColors_,
          getKeyboardBacklightProvider(), this.getStore());
    }
  }

  private getZoneTabIndex_(zoneIdx: number): string {
    return zoneIdx === 0 ? '0' : '1';
  }

  private getZoneAriaChecked_(zoneIdx: number, zoneSelected: number) {
    return (zoneIdx === zoneSelected).toString();
  }

  private getZoneTitle_(zoneIdx: number) {
    return loadTimeData.getStringF('zoneTitle', zoneIdx + 1);
  }

  private getSelectedColor_(zoneSelected: number, zoneColors: BacklightColor[]):
      BacklightColor|null {
    return zoneColors ? zoneColors[zoneSelected] : null;
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

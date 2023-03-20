// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import './color_icon_element.js';
import '../../css/common.css.js';
import '../../css/cros_button_style.css.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {BacklightColor, CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {isMultiZoneRgbKeyboardSupported} from '../load_time_booleans.js';
import {logKeyboardBacklightOpenZoneCustomizationUMA} from '../personalization_metrics_logger.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {ColorInfo, getPresetColors, RAINBOW, WALLPAPER} from '../utils.js';

import {PresetColorSelectedEvent} from './color_selector_element.js';
import {setBacklightColor} from './keyboard_backlight_controller.js';
import {getTemplate} from './keyboard_backlight_element.html.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';
import {KeyboardBacklightObserver} from './keyboard_backlight_observer.js';
import {ZoneCustomizationElement} from './zone_customization_element.js';


/**
 * @fileoverview
 * The keyboard backlight section that allows users to customize their keyboard
 * backlight colors.
 */

export interface KeyboardBacklight {
  $: {
    zoneCustomizationRender: CrLazyRenderElement<ZoneCustomizationElement>,
  };
}

export class KeyboardBacklight extends WithPersonalizationStore {
  static get is() {
    return 'keyboard-backlight';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isMultiZoneRgbKeyboardSupported_: {
        type: Boolean,
        value() {
          return isMultiZoneRgbKeyboardSupported();
        },
      },

      presetColors_: {
        type: Object,
        value() {
          return getPresetColors();
        },
      },

      presetColorIds_: {
        type: Array,
        computed: 'computePresetColorIds_(presetColors_)',
      },

      rainbowColorId_: {
        type: String,
        value: RAINBOW,
      },

      wallpaperColorId_: {
        type: String,
        value: WALLPAPER,
      },

      backlightColor_: {
        type: Object,
        computed: 'computeBacklightColor_(currentBacklightState_)',
      },

      /** The color currently highlighted by keyboard navigation. */
      ironSelectedColor_: Object,

      /** The current backlight state in the system. */
      currentBacklightState_: Object,

      /** The current wallpaper extracted color. */
      wallpaperColor_: Object,
    };
  }

  private isMultiZoneRgbKeyboardSupported_: boolean;
  private presetColors_: Record<string, ColorInfo>;
  private presetColorIds_: string[];
  private backlightColor_: BacklightColor|null|undefined;
  private rainbowColorId_: string;
  private wallpaperColorId_: string;
  private ironSelectedColor_: HTMLElement;
  private currentBacklightState_: CurrentBacklightState|null;
  private wallpaperColor_: SkColor|null;

  override connectedCallback() {
    super.connectedCallback();
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
    this.watch<KeyboardBacklight['currentBacklightState_']>(
        'currentBacklightState_',
        state => state.keyboardBacklight.currentBacklightState);
    this.watch<KeyboardBacklight['wallpaperColor_']>(
        'wallpaperColor_', state => state.keyboardBacklight.wallpaperColor);
    this.updateFromStore();
  }

  private computePresetColorIds_(presetColors: Record<string, string>):
      string[] {
    // ES2020 maintains ordering of Object.keys.
    return Object.keys(presetColors);
  }

  private computeBacklightColor_(currentBacklightState: CurrentBacklightState):
      BacklightColor|null|undefined {
    return currentBacklightState ? currentBacklightState.color : null;
  }

  /** Invoked when the wallpaper color is selected. */
  private onWallpaperColorSelected_() {
    setBacklightColor(
        BacklightColor.kWallpaper, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when a preset color is selected. */
  private onPresetColorSelected_(e: PresetColorSelectedEvent) {
    const colorId = e.detail.colorId;
    assert(colorId !== undefined, 'colorId not found');
    setBacklightColor(
        this.presetColors_[colorId].enumVal, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when the rainbow color is selected. */
  private onRainbowColorSelected_() {
    setBacklightColor(
        BacklightColor.kRainbow, getKeyboardBacklightProvider(),
        this.getStore());
  }

  private showZoneCustomizationDialog_() {
    assert(
        this.isMultiZoneRgbKeyboardSupported_,
        'zone customization dialog only available if multi-zone is supported');
    logKeyboardBacklightOpenZoneCustomizationUMA();
    this.$.zoneCustomizationRender.get().showModal();
  }

  private getZoneCustomizationButtonAriaPressed_(
      currentBacklightState: CurrentBacklightState): string {
    return (!!currentBacklightState && !!currentBacklightState.zoneColors)
        .toString();
  }
}

customElements.define(KeyboardBacklight.is, KeyboardBacklight);

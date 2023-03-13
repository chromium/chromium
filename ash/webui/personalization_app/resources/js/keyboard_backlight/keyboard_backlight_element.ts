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
import {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {BacklightColor, CurrentBacklightState} from '../../personalization_app.mojom-webui.js';
import {isMultiZoneRgbKeyboardSupported} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {ColorInfo, getPresetColors, isSelectionEvent, RAINBOW, WALLPAPER} from '../utils.js';

import {getShouldShowNudge, handleNudgeShown, setBacklightColor} from './keyboard_backlight_controller.js';
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
    keys: IronA11yKeysElement,
    selector: IronSelectorElement,
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
        computed: 'computePresetColors_()',
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

      shouldShowNudge_: {
        type: Boolean,
        value: false,
        observer: 'onShouldShowNudgeChanged_',
      },
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
  private shouldShowNudge_: boolean;

  override ready() {
    super.ready();
    this.$.keys.target = this.$.selector;
  }

  override connectedCallback() {
    super.connectedCallback();
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
    this.watch<KeyboardBacklight['currentBacklightState_']>(
        'currentBacklightState_',
        state => state.keyboardBacklight.currentBacklightState);
    this.watch<KeyboardBacklight['shouldShowNudge_']>(
        'shouldShowNudge_', state => state.keyboardBacklight.shouldShowNudge);
    this.watch<KeyboardBacklight['wallpaperColor_']>(
        'wallpaperColor_', state => state.keyboardBacklight.wallpaperColor);
    this.updateFromStore();

    getShouldShowNudge(getKeyboardBacklightProvider(), this.getStore());
  }

  private computePresetColors_(): Record<string, ColorInfo> {
    return getPresetColors();
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

  /** Handle keyboard navigation. */
  private onKeysPress_(
      e: CustomEvent<{key: string, keyboardEvent: KeyboardEvent}>) {
    const selector = this.$.selector;
    const prevButton = this.ironSelectedColor_;
    switch (e.detail.key) {
      case 'left':
        selector.selectPrevious();
        break;
      case 'right':
        selector.selectNext();
        break;
      case 'enter':
        switch (this.ironSelectedColor_.id) {
          case this.rainbowColorId_:
            this.onRainbowColorSelected_(e.detail.keyboardEvent);
            break;
          case this.wallpaperColorId_:
            this.onWallpaperColorSelected_(e.detail.keyboardEvent);
            break;
          default:
            // |onPresetColorSelected_| is not invoked here because the event
            // listener target is iron-selector, which results in undefined
            // colorId.
            setBacklightColor(
                this.presetColors_[this.ironSelectedColor_.id].enumVal,
                getKeyboardBacklightProvider(), this.getStore());
            break;
        }
        break;
      default:
        return;
    }
    // Remove focus state of previous button.
    if (prevButton) {
      prevButton.removeAttribute('tabindex');
    }
    // Add focus state for new button.
    if (this.ironSelectedColor_) {
      this.ironSelectedColor_.setAttribute('tabindex', '0');
      this.ironSelectedColor_.focus();
    }
    e.detail.keyboardEvent.preventDefault();
  }

  /** Invoked when the wallpaper color is selected. */
  private onWallpaperColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    setBacklightColor(
        BacklightColor.kWallpaper, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when a preset color is selected. */
  private onPresetColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const htmlElement = e.currentTarget as HTMLElement;
    const colorId = htmlElement.id;
    assert(colorId !== undefined, 'colorId not found');
    setBacklightColor(
        this.presetColors_[colorId].enumVal, getKeyboardBacklightProvider(),
        this.getStore());
  }

  /** Invoked when the rainbow color is selected. */
  private onRainbowColorSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    setBacklightColor(
        BacklightColor.kRainbow, getKeyboardBacklightProvider(),
        this.getStore());
  }

  private getPresetColorAriaLabel_(presetColorId: string): string {
    return this.i18n(presetColorId);
  }

  private getWallpaperColorContainerClass_(selectedColor: BacklightColor):
      string {
    return this.getColorContainerClass_(
        this.getWallpaperColorAriaChecked_(selectedColor));
  }

  private getPresetColorContainerClass_(
      colorId: string, colors: Record<string, ColorInfo>,
      selectedColor: BacklightColor) {
    return this.getColorContainerClass_(
        this.getPresetColorAriaChecked_(colorId, colors, selectedColor));
  }

  private getRainbowColorContainerClass_(selectedColor: BacklightColor) {
    return this.getColorContainerClass_(
        this.getRainbowColorAriaChecked_(selectedColor));
  }

  private getColorContainerClass_(isSelected: string) {
    const defaultClassName = 'selectable';
    return isSelected === 'true' ? `${defaultClassName} tast-selected-color` :
                                   defaultClassName;
  }

  private getWallpaperColorAriaChecked_(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kWallpaper).toString();
  }

  private getPresetColorAriaChecked_(
      colorId: string, colors: Record<string, ColorInfo>,
      selectedColor: BacklightColor) {
    if (!colorId || !colors[colorId]) {
      return 'false';
    }
    return (colors[colorId].enumVal === selectedColor).toString();
  }

  private getRainbowColorAriaChecked_(selectedColor: BacklightColor) {
    return (selectedColor === BacklightColor.kRainbow).toString();
  }

  private getWallpaperColorTitle_() {
    return this.i18n('wallpaperColorTooltipText');
  }

  private onShouldShowNudgeChanged_(shouldShowNudge: boolean) {
    if (shouldShowNudge) {
      setTimeout(() => {
        handleNudgeShown(getKeyboardBacklightProvider(), this.getStore());
      }, 3000);
    }
  }

  private showZoneCustomizationDialog_() {
    assert(
        this.isMultiZoneRgbKeyboardSupported_,
        'zone customization dialog only available if multi-zone is supported');
    this.$.zoneCustomizationRender.get().showModal();
  }

  private getZoneCustomizationButtonAriaPressed_(
      currentBacklightState: CurrentBacklightState): string {
    return (!!currentBacklightState && !!currentBacklightState.zoneColors)
        .toString();
  }
}

customElements.define(KeyboardBacklight.is, KeyboardBacklight);

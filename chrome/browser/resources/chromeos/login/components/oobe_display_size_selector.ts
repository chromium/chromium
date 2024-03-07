// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {CrSliderElement} from '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {getTemplate} from './oobe_display_size_selector.html.js';

const DEFAULT_APP_ICON_SIZE = 48;
const DEFAULT_FONT_SIZE = 14;

const OobeDisplaySizeSelectorBase = OobeI18nMixin(PolymerElement);

interface App {
  icon: string;
  name: string;
}

interface SizeTick {
  value: number;
  ariaValue: number;
  label: string;
}

export class OobeDisplaySizeSelector extends OobeDisplaySizeSelectorBase {
  static get is() {
    return 'oobe-display-size-selector' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of available display sizes.
       */
      availableSizesTicks: {
        type: Array,
      },

      /* Count of the markers in the display size slider, calculated in init()
      method. */
      markerCounts: {
        type: Number,
      },

      tickedSizeIndex: {
        type: Number,
        observer: 'updatePreviewSizes',
      },

      /**
       * List of apps icons and names..
       */
      apps: {
        type: Array,
      },
    };
  }

  private availableSizesTicks: SizeTick[];
  private markerCounts: number;
  private tickedSizeIndex: number;
  private apps: App[];

  /**
   * Initialize the list of screens.
   */
  init(availableSizes: number[], currentSize: number): void {
    let currentSizeIndex = 0;
    let minDiff = Math.abs(availableSizes[0] - currentSize);

    const sliderSizeTicks: SizeTick[] = [];
    for (let i = 0; i < availableSizes.length; i++) {
      const ariaValue = Math.round(availableSizes[i] * 100);
      const sizeTick: SizeTick = {
        value: availableSizes[i],
        ariaValue,
        label: this.i18n('displaySizeValue', ariaValue.toString()),
      };
      sliderSizeTicks.push(sizeTick);

      if (minDiff > Math.abs(availableSizes[i] - currentSize)) {
        currentSizeIndex = i;
        minDiff = Math.abs(availableSizes[i] - currentSize);
      }
    }

    if (document.querySelector('html[dir=rtl]')) {
      this.getSizeSlider().setAttribute('is-rtl_', '');
    }

    this.availableSizesTicks = sliderSizeTicks;
    this.tickedSizeIndex = currentSizeIndex;
    this.markerCounts = this.availableSizesTicks.length;

    this.apps = [
      {icon: 'images/app_icons/files.svg', name: 'displaySizeFilesApp'},
      {icon: 'images/app_icons/photos.svg', name: 'displaySizePhotosApp'},
      {
        icon: 'images/app_icons/calculator.svg',
        name: 'displaySizeCalculatorApp',
      },
      {icon: 'images/app_icons/camera.svg', name: 'displaySizeCameraApp'},
      {icon: 'images/app_icons/settings.svg', name: 'displaySizeSettingsApp'},
      {icon: 'images/app_icons/a4.svg', name: 'displaySizeA4App'},
    ];
  }

  /**
   * Returns the selected display size.
   */
  getSelectedSize(): number {
    return this.availableSizesTicks[this.tickedSizeIndex].value;
  }

  // Called upon `tickedSizeIndex` property changes.
  private updatePreviewSizes(): void {
    const selectedSize = this.availableSizesTicks[this.tickedSizeIndex].value;
    const icons = this.shadowRoot?.querySelectorAll<HTMLElement>('.app-icon');
    assert(icons);
    for (let i = 0; i < icons.length; i++) {
      icons[i].style.width = selectedSize * DEFAULT_APP_ICON_SIZE + 'px';
    }

    const title = this.shadowRoot?.querySelector<HTMLElement>('#previewTitle');
    assert(title instanceof HTMLElement);
    title.style.fontSize = selectedSize * DEFAULT_FONT_SIZE + 'px';

    const names = this.shadowRoot?.querySelectorAll<HTMLElement>('.app-name');
    assert(names);
    for (let i = 0; i < names.length; i++) {
      names[i].style.fontSize = selectedSize * DEFAULT_FONT_SIZE + 'px';
    }
  }

  private getSizeSlider(): CrSliderElement {
    const slider =
        this.shadowRoot?.querySelector<CrSliderElement>('#sizeSlider');
    assert(slider instanceof CrSliderElement);
    return slider;
  }

  private onTickedSizeChanged(): void {
    this.tickedSizeIndex = this.getSizeSlider().value;
  }

  private onPositiveClicked(): void {
    if (this.getSizeSlider().value + 1 < this.markerCounts) {
      this.getSizeSlider().value += 1;
      this.tickedSizeIndex += 1;
    }
  }

  private onNegativeClicked(): void {
    if (this.getSizeSlider().value >= 1) {
      this.getSizeSlider().value -= 1;
      this.tickedSizeIndex -= 1;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeDisplaySizeSelector.is]: OobeDisplaySizeSelector;
  }
}

customElements.define(OobeDisplaySizeSelector.is, OobeDisplaySizeSelector);

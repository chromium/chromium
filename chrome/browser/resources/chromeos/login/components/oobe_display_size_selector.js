// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/cr_elements/icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';

const DEFAULT_APP_ICON_SIZE = 48;
const DEFAULT_FONT_SIZE = 14;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeDisplaySizeSelectorBase =
    mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class OobeDisplaySizeSelector extends OobeDisplaySizeSelectorBase {
  static get is() {
    return 'oobe-display-size-selector';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of available display sizes.
       * @type {!Array<number>}
       */
      availableSizesTicks_: {
        type: Array,
      },

      /* Count of the markers in the display size slider, calculated in init()
      method. */
      markerCounts_: {
        type: Number,
      },

      tickedSizeIndex_: {
        type: Number,
        observer: 'updatePreviewSizes_',
      },

      /**
       * List of apps icons and names..
       */
      apps_: {
        type: Array,
      },
    };
  }

  /**
   * Initialize the list of screens.
   */
  init(availableSizes, currentSize) {
    let currentSizeIndex = 0;
    let minDiff = Math.abs(availableSizes[0] - currentSize);

    const sliderSizeTicks = [];
    for (let i = 0; i < availableSizes.length; i++) {
      const ariaValue = Math.round(availableSizes[i] * 100);
      sliderSizeTicks.push({
        value: availableSizes[i],
        ariaValue,
        label: this.i18n('displaySizeValue', ariaValue.toString()),
      });

      if (minDiff > Math.abs(availableSizes[i] - currentSize)) {
        currentSizeIndex = i;
        minDiff = Math.abs(availableSizes[i] - currentSize);
      }
    }

    if (document.querySelector('html[dir=rtl]')) {
      this.shadowRoot.querySelector('#sizeSlider').setAttribute('is-rtl_', '');
    }

    this.availableSizesTicks_ = sliderSizeTicks;
    this.tickedSizeIndex_ = currentSizeIndex;
    this.markerCounts_ = this.availableSizesTicks_.length;

    this.apps_ = [
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
   * @returns {!number}
   */
  getSelectedSize() {
    return this.availableSizesTicks_[this.tickedSizeIndex_].value;
  }

  // Called upon `tickedSizeIndex_` property changes.
  updatePreviewSizes_() {
    const selectedSize = this.availableSizesTicks_[this.tickedSizeIndex_].value;
    const icons = this.shadowRoot.querySelectorAll('.app-icon');
    for (var i = 0; i < icons.length; i++) {
      icons[i].style.width = selectedSize * DEFAULT_APP_ICON_SIZE + 'px';
    }

    const title = this.shadowRoot.querySelector('#previewTitle');
    title.style.fontSize = selectedSize * DEFAULT_FONT_SIZE + 'px';

    const names = this.shadowRoot.querySelectorAll('.app-name');
    for (var i = 0; i < names.length; i++) {
      names[i].style.fontSize = selectedSize * DEFAULT_FONT_SIZE + 'px';
    }
  }

  onTickedSizeChanged_() {
    this.tickedSizeIndex_ = this.$.sizeSlider.value;
  }
}

customElements.define(OobeDisplaySizeSelector.is, OobeDisplaySizeSelector);

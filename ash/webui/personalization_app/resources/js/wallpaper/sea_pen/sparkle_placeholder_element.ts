// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays a sparkle effect.
 */

import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Sparkle} from '../../surface_effects/sparkle.js';
import {parseCssColor} from '../../surface_effects/utils.js';

import {getTemplate} from './sparkle_placeholder_element.html.js';

export class SparklePlaceholderElement extends PolymerElement {
  static get is() {
    return 'sparkle-placeholder-element';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether dark mode is the active preferred color scheme.
       */
      isDarkModeActive: {
        type: Boolean,
        value: false,
      },

      index: {
        type: Number,
        observer: 'onIndexChanged_',
      },
    };
  }

  isDarkModeActive: boolean;
  index: number;
  sparkleImpl = new Sparkle();

  override connectedCallback() {
    super.connectedCallback();

    try {
      this.sparkleImpl.initialize();
    } catch (error: unknown) {
      console.error(error);
    }

    this.initSparkle_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.shadowRoot != null &&
        this.sparkleImpl.element.parentNode === this.shadowRoot) {
      this.sparkleImpl.stopRendering();
    }
  }

  private onIndexChanged_(index: number) {
    setTimeout(() => {
      this.sparkleImpl.applyNoiseOffset();
    }, index * 500);
  }

  private initSparkle_() {
    const sparkleElement = this.sparkleImpl.element;

    if (this.shadowRoot != null &&
        sparkleElement.parentNode !== this.shadowRoot) {
      this.shadowRoot.appendChild(sparkleElement);
      this.sparkleImpl.startRendering();
    }
    if (this.isDarkModeActive) {
      this.sparkleImpl.setTopLeftBackgroundColor(parseCssColor('#344477'));
      this.sparkleImpl.setBottomRightBackgroundColor(parseCssColor('#002116'));
    } else {
      this.sparkleImpl.setTopLeftBackgroundColor(parseCssColor('#B5C4FF'));
      this.sparkleImpl.setBottomRightBackgroundColor(parseCssColor('#B3EFD4'));
    }
    this.sparkleImpl.setSparkleColor(parseCssColor('#FFFFFF'));
    this.sparkleImpl.setInverseNoiseLuminosity(true);
    this.sparkleImpl.setGridCount(1.7);
    this.sparkleImpl.setLumaMatteFactors();
    this.sparkleImpl.setOpacity(1.);

    const rect = this.sparkleImpl.element.getBoundingClientRect();
    this.sparkleImpl.resize(rect.width, rect.height);
  }
}

customElements.define(SparklePlaceholderElement.is, SparklePlaceholderElement);

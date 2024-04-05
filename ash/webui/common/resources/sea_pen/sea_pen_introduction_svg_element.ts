// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the illutration in the Sea
 * Pen introduction dialog for Wallpaper app.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sea_pen_introduction_svg_element.html.js';
import {isPersonalizationApp} from './sea_pen_utils.js';

export class SeaPenIntroductionSvgElement extends PolymerElement {
  static get is() {
    return 'sea-pen-introduction-svg';
  }

  static get properties() {
    return {
      isPersonalizationApp_: {
        type: Boolean,
        value() {
          return isPersonalizationApp();
        },
      },
    };
  }

  static get template() {
    return getTemplate();
  }

  private isPersonalizationApp_: boolean;
}

customElements.define(
    SeaPenIntroductionSvgElement.is, SeaPenIntroductionSvgElement);

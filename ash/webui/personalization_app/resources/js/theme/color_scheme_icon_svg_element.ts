// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component is for the color scheme icon svg.
 */

import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {SampleColorScheme} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {convertToRgbHexStr} from '../utils.js';

import {getTemplate} from './color_scheme_icon_svg_element.html.js';

export class ColorSchemeIconSvgElement extends WithPersonalizationStore {
  static get is() {
    return 'color-scheme-icon-svg';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      scheme_: {
        type: Object,
        readOnly: true,
      },
    };
  }
  private scheme_: SampleColorScheme;

  private getHexColor_(color: SkColor): string {
    if (!color || !color.value) {
      return 'none';
    }
    return convertToRgbHexStr(color.value);
  }
}

customElements.define(ColorSchemeIconSvgElement.is, ColorSchemeIconSvgElement);

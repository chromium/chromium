// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './color.html.js';

export interface ColorElement {
  $: {
    background: Element,
    foreground: Element,
  };
}

export class ColorElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-color';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      backgroundColor: {
        type: Object,
        value: 0,
        observer: 'onColorChange_',
      },
      foregroundColor: {
        type: Object,
        value: 0,
        observer: 'onColorChange_',
      },
    };
  }

  public backgroundColor: SkColor;
  public foregroundColor: SkColor;

  private onColorChange_() {
    this.updateStyles({
      '--customize-chrome-color-foreground-color':
          skColorToRgba(this.foregroundColor),
      '--customize-chrome-color-background-color':
          skColorToRgba(this.backgroundColor),
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-color': ColorElement;
  }
}

customElements.define(ColorElement.is, ColorElement);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText, Color} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import {hexToColor} from '../pdf_viewer_utils.js';

import type {ColorOption} from './ink_color_selector.js';
import {InkTextObserverMixin} from './ink_text_observer_mixin.js';
import {getCss} from './viewer_text_side_panel.css.js';
import {getHtml} from './viewer_text_side_panel.html.js';

const TEXT_SIZES: number[] =
    [6, 8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64, 72, 100];

// TODO(crbug.com/402547554): Fix labels to correspond to colors, and add
// the labels to pdf_strings.grdp once they are available from UX. Some
// "blue" colors are actually teal, "Yellow3" and "Red2" are subtly different
// from the corresponding brush colors.
export const TEXT_COLORS: ColorOption[] = [
  // Row 1:
  {label: 'annotationColorBlack', color: '#000000', blended: false},
  {label: 'ink2BrushColorDarkGrey2', color: '#5f6368', blended: false},
  {label: 'ink2BrushColorDarkGrey1', color: '#9aa0a6', blended: false},
  {label: 'annotationColorLightGrey', color: '#dadce0', blended: false},
  {label: 'annotationColorWhite', color: '#ffffff', blended: false},
  // Row 2:
  {label: 'ink2BrushColorRed1', color: '#f28b82', blended: false},
  {label: 'ink2BrushColorYellow1', color: '#fdd663', blended: false},
  {label: 'ink2BrushColorGreen1', color: '#81c995', blended: false},
  {label: 'ink2BrushColorBlue1', color: '#78d9ec', blended: false},
  {label: 'ink2BrushColorBlue1', color: '#8ab4f8', blended: false},
  // Row 3:
  {label: 'ink2BrushColorRed2', color: '#e94235', blended: false},
  {label: 'ink2BrushColorYellow2', color: '#fbbc04', blended: false},
  {label: 'ink2BrushColorGreen2', color: '#34a853', blended: false},
  {label: 'ink2BrushColorBlue2', color: '#24c1e0', blended: false},
  {label: 'ink2BrushColorBlue2', color: '#4285f4', blended: false},
  // Row 4:
  {label: 'ink2BrushColorRed3', color: '#c5221f', blended: false},
  {label: 'ink2BrushColorYellow3', color: '#d56e0c', blended: false},
  {label: 'ink2BrushColorGreen3', color: '#188038', blended: false},
  {label: 'ink2BrushColorBlue3', color: '#12a4af', blended: false},
  {label: 'ink2BrushColorBlue3', color: '#1967d2', blended: false},
];

const ViewerTextSidePanelElementBase = InkTextObserverMixin(CrLitElement);

export class ViewerTextSidePanelElement extends ViewerTextSidePanelElementBase {
  static get is() {
    return 'viewer-text-side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentColor_: {type: Object},
      currentFont_: {type: String},
      currentSize_: {type: Number},
      colors_: {type: Array},
      fonts_: {type: Array},
      sizes_: {type: Array},
    };
  }

  protected currentColor_: Color = hexToColor(TEXT_COLORS[0]!.color);
  protected currentFont_: string = '';
  protected currentSize_: number = TEXT_SIZES[0]!;

  protected colors_ = TEXT_COLORS;
  protected fonts_ = [
    'Roboto',
    'Serif',
    'Sans',
    'Monospace',
  ];
  protected sizes_ = TEXT_SIZES;

  protected isSelectedFont_(font: string) {
    return font === this.currentFont_;
  }

  protected isSelectedSize_(size: number) {
    return size === this.currentSize_;
  }

  protected onFontChange_(e: Event) {
    const newValue = (e.target as HTMLSelectElement).value;
    Ink2Manager.getInstance().setTextFont(newValue);
  }

  protected onSizeChange_(e: Event) {
    const newValue = Number((e.target as HTMLSelectElement).value);
    Ink2Manager.getInstance().setTextSize(newValue);
  }

  protected onCurrentColorChanged_(e: CustomEvent<{value: Color}>) {
    // Avoid poking the plugin if the value hasn't actually changed.
    const newColor = e.detail.value;
    if (newColor.r !== this.currentColor_.r ||
        newColor.b !== this.currentColor_.b ||
        newColor.g !== this.currentColor_.g) {
      Ink2Manager.getInstance().setTextColor(newColor);
    }
  }

  override onTextChanged(text: AnnotationText) {
    this.currentColor_ = text.color;
    this.currentFont_ = text.font;
    this.currentSize_ = text.size;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-text-side-panel': ViewerTextSidePanelElement;
  }
}

customElements.define(
    ViewerTextSidePanelElement.is, ViewerTextSidePanelElement);

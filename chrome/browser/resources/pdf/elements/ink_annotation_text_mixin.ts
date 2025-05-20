// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Color, TextAttributes} from '../constants.js';
import {TextTypeface} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import {hexToColor} from '../pdf_viewer_utils.js';

import type {ColorOption} from './ink_color_selector.js';

type Constructor<T> = new (...args: any[]) => T;

export const TEXT_SIZES: number[] =
    [6, 8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64, 72, 100];

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
  {label: 'ink2TextColorCyan1', color: '#78d9ec', blended: false},
  {label: 'ink2BrushColorBlue1', color: '#8ab4f8', blended: false},
  // Row 3:
  {label: 'ink2BrushColorRed2', color: '#e94235', blended: false},
  {label: 'ink2BrushColorYellow2', color: '#fbbc04', blended: false},
  {label: 'ink2BrushColorGreen2', color: '#34a853', blended: false},
  {label: 'ink2TextColorCyan2', color: '#24c1e0', blended: false},
  {label: 'ink2BrushColorBlue2', color: '#4285f4', blended: false},
  // Row 4:
  {label: 'ink2BrushColorRed3', color: '#c5221f', blended: false},
  {label: 'ink2BrushColorYellow3', color: '#d56e0c', blended: false},
  {label: 'ink2BrushColorGreen3', color: '#188038', blended: false},
  {label: 'ink2TextColorCyan3', color: '#12a4af', blended: false},
  {label: 'ink2BrushColorBlue3', color: '#1967d2', blended: false},
];

export const InkAnnotationTextMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<InkAnnotationTextMixinInterface> => {
      class InkAnnotationTextMixin extends superClass implements
          InkAnnotationTextMixinInterface {
        static get properties() {
          return {
            colors: {type: Array},
            currentColor: {type: Object},
            currentSize: {type: Number},
            currentTypeface: {type: String},
            fontNames: {type: Array},
            sizes: {type: Array},
          };
        }

        accessor currentColor: Color = hexToColor(TEXT_COLORS[0]!.color);
        accessor currentSize: number = TEXT_SIZES[3]!;
        accessor currentTypeface: TextTypeface = TextTypeface.SANS_SERIF;
        accessor colors: ColorOption[] = TEXT_COLORS;
        accessor fontNames: TextTypeface[] = [
          TextTypeface.SANS_SERIF,
          TextTypeface.SERIF,
          TextTypeface.MONOSPACE,
        ];
        accessor sizes: number[] = TEXT_SIZES;

        getLabelForTypeface(typeface: TextTypeface): string {
          switch (typeface) {
            case TextTypeface.SANS_SERIF:
              return 'ink2TextFontSansSerif';
            case TextTypeface.SERIF:
              return 'ink2TextFontSerif';
            case TextTypeface.MONOSPACE:
              return 'ink2TextFontMonospace';
          }
        }

        isSelectedTypeface(typeface: TextTypeface): boolean {
          return typeface === this.currentTypeface;
        }

        isSelectedSize(size: number): boolean {
          return size === this.currentSize;
        }

        onTypefaceSelected(e: Event) {
          const newValue = (e.target as HTMLSelectElement).value;
          Ink2Manager.getInstance().setTextTypeface(newValue as TextTypeface);
        }

        onSizeSelected(e: Event) {
          const newValue = Number((e.target as HTMLSelectElement).value);
          Ink2Manager.getInstance().setTextSize(newValue);
        }

        onCurrentColorChanged(e: CustomEvent<{value: Color}>) {
          // Avoid poking the plugin if the value hasn't actually changed.
          const newColor = e.detail.value;
          if (newColor.r !== this.currentColor.r ||
              newColor.b !== this.currentColor.b ||
              newColor.g !== this.currentColor.g) {
            Ink2Manager.getInstance().setTextColor(newColor);
          }
        }

        onTextAttributesChanged(attributes: TextAttributes) {
          this.currentColor = attributes.color;
          this.currentTypeface = attributes.typeface;
          this.currentSize = attributes.size;
        }
      }
      return InkAnnotationTextMixin;
    };

export interface InkAnnotationTextMixinInterface {
  colors: ColorOption[];
  currentColor: Color;
  currentSize: number;
  currentTypeface: string;
  fontNames: TextTypeface[];
  sizes: number[];
  getLabelForTypeface(typeface: TextTypeface): string;
  isSelectedTypeface(typeface: string): boolean;
  isSelectedSize(size: number): boolean;
  onTypefaceSelected(e: Event): void;
  onCurrentColorChanged(e: CustomEvent<{value: Color}>): void;
  onSizeSelected(e: CustomEvent<{value: number}>): void;
  onTextAttributesChanged(attributes: TextAttributes): void;
}

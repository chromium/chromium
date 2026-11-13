// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import type {CrLitElement, PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Color, ColorOption, TextAttributes} from '../constants.js';
import {TEXT_AND_SIGNATURE_COLORS, TextTypeface} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import {colorsEqual, hexToColor} from '../pdf_viewer_utils.js';

type Constructor<T> = new (...args: any[]) => T;

// LINT.IfChange(TextSizes)
export const TEXT_SIZES: number[] =
    [6, 8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64, 72, 100];
// LINT.ThenChange(//pdf/pdfium/pdfium_ink_reader.cc:TextSizes)

export const TEXT_COLORS: ColorOption[] = TEXT_AND_SIGNATURE_COLORS;

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

        override firstUpdated(changedProperties: PropertyValues<this>) {
          super.firstUpdated(changedProperties);
          const typefaceSelect =
              this.shadowRoot.querySelector<HTMLSelectElement>(
                  '#typefaceSelect');
          assert(typefaceSelect);
          typefaceSelect.value = this.currentTypeface;

          const sizeSelect =
              this.shadowRoot.querySelector<HTMLSelectElement>('#sizeSelect');
          assert(sizeSelect);
          sizeSelect.value = this.currentSize.toString();
        }

        getLabelForTypeface(typeface: TextTypeface): string {
          switch (typeface) {
            case TextTypeface.SANS_SERIF:
              return 'ink2TextFontSansSerif';
            case TextTypeface.SERIF:
              return 'ink2TextFontSerif';
            case TextTypeface.MONOSPACE:
              return 'ink2TextFontMonospace';
            default:
              assertNotReachedCase(typeface);
          }
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
          if (!colorsEqual(newColor, this.currentColor)) {
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
  onTypefaceSelected(e: Event): void;
  onCurrentColorChanged(e: CustomEvent<{value: Color}>): void;
  onSizeSelected(e: Event): void;
  onTextAttributesChanged(attributes: TextAttributes): void;
}

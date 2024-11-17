// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {Color} from '../constants.js';
import {blendHighlighterColorValue, colorToHex, hexToColor} from '../pdf_viewer_utils.js';

import {getCss} from './ink_color_selector.css.js';
import {getHtml} from './ink_color_selector.html.js';

const NUM_OPTION_COLUMNS: number = 5;

interface ColorOption {
  name: string;
  color: string;
}

const HIGHLIGHTER_COLORS: ColorOption[] = [
  // Row 1:
  {name: 'highlighterColorRed300', color: '#f28b82'},
  {name: 'highlighterColorYellow300', color: '#fdd663'},
  {name: 'highlighterColorGreen300', color: '#34a853'},
  {name: 'highlighterColorBlue', color: '#4285f4'},
  {name: 'highlighterColorOrange', color: '#ffae80'},
  // Row 2:
  {name: 'highlighterColorRed600', color: '#d93025'},
  {name: 'highlighterColorLemon400', color: '#ddf300'},
  {name: 'highlighterColorAloe400', color: '#25e387'},
  {name: 'highlighterColorIndigo', color: '#5379ff'},
  {name: 'highlighterColorOrange', color: '#ff630c'},
];

const PEN_COLORS: ColorOption[] = [
  // Row 1:
  {name: 'penColorBlack', color: '#000000'},
  {name: 'penColorGrey700', color: '#5f6368'},
  {name: 'penColorGrey500', color: '#9aa0a6'},
  {name: 'penColorGrey300', color: '#dadce0'},
  {name: 'penColorWhite', color: '#ffffff'},
  // Row 2:
  {name: 'penColorRed300', color: '#f28b82'},
  {name: 'penColorYellow300', color: '#fdd663'},
  {name: 'penColorGreen300', color: '#81c995'},
  {name: 'penColorBlue300', color: '#8ab4f8'},
  {name: 'penColorBrown1', color: '#eec9ae'},
  // Row 3:
  {name: 'penColorRed500', color: '#ea4335'},
  {name: 'penColorYellow500', color: '#fbbc04'},
  {name: 'penColorGreen500', color: '#34a853'},
  {name: 'penColorBlue500', color: '#4285f4'},
  {name: 'penColorBrown2', color: '#e2a185'},
  // Row 4:
  {name: 'penColorRed700', color: '#c5221f'},
  {name: 'penColorYellow700', color: '#f29900'},
  {name: 'penColorGreen700', color: '#188038'},
  {name: 'penColorBlue700', color: '#1967d2'},
  {name: 'penColorBrown3', color: '#885945'},
];

/**
 * @returns Whether `lhs` and `rhs` have the same RGB values or not.
 */
function areColorsEqual(lhs: Color, rhs: Color): boolean {
  return lhs.r === rhs.r && lhs.g === rhs.g && lhs.b === rhs.b;
}

/**
 * Given an arrow key, the index of the current selected color, and the number
 * of color options, returns the index of the color that should be selected
 * after moving in the direction of the arrow key in a 2D grid of color options.
 * @param key The key pressed. Must be an arrow key.
 * @param currentIndex The index of the current selected color.
 * @param numOptions The number of color options.
 * @returns The index of the color that should be selected after moving in the
 *     direction of `key`.
 */
function getNewColorIndex(
    key: string, currentIndex: number, numOptions: number): number {
  let delta: number;
  switch (key) {
    case 'ArrowLeft':
      // If the current index is in the first column, wrap to the last column.
      // Otherwise, move one column left.
      delta = (currentIndex % NUM_OPTION_COLUMNS === 0) ?
          NUM_OPTION_COLUMNS - 1 :
          -1;
      break;
    case 'ArrowUp':
      // If the current index is in the first row, wrap to the last row.
      // Otherwise, move one row up.
      delta = (currentIndex < NUM_OPTION_COLUMNS) ?
          numOptions - NUM_OPTION_COLUMNS :
          -NUM_OPTION_COLUMNS;
      break;
    case 'ArrowRight':
      // If the current index is in the last column, wrap to the first column.
      // Otherwise, move one column right.
      delta = (currentIndex % NUM_OPTION_COLUMNS === NUM_OPTION_COLUMNS - 1) ?
          -NUM_OPTION_COLUMNS + 1 :
          1;
      break;
    case 'ArrowDown':
      // If the current index is in the last row, wrap to the first row.
      // Otherwise, move one row down.
      delta = (currentIndex >= numOptions - NUM_OPTION_COLUMNS) ?
          -numOptions + NUM_OPTION_COLUMNS :
          NUM_OPTION_COLUMNS;
      break;
    default:
      assertNotReached();
  }
  return currentIndex + delta;
}

export class InkColorSelectorElement extends CrLitElement {
  static get is() {
    return 'ink-color-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentColor: {
        notify: true,
        type: Object,
      },
      currentType: {type: String},
    };
  }

  currentColor: Color = {r: 0, g: 0, b: 0};
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected onColorClick_(e: Event) {
    this.setBrushColor_(e.currentTarget as HTMLInputElement);
  }

  protected onColorKeydown_(e: KeyboardEvent) {
    // Only handle arrow keys.
    if (e.key !== 'ArrowLeft' && e.key !== 'ArrowUp' &&
        e.key !== 'ArrowRight' && e.key !== 'ArrowDown') {
      return;
    }
    e.preventDefault();

    const colorButton = e.target as HTMLInputElement;
    const currentIndex = Number(colorButton.dataset['index']);

    const brushColors = this.getCurrentBrushColors_();
    const numOptions = brushColors.length;
    const newIndex = getNewColorIndex(e.key, currentIndex, numOptions);
    assert(newIndex >= 0);
    assert(newIndex < numOptions);

    const newColor = brushColors[newIndex]!.color;
    const newColorButton = this.shadowRoot!.querySelector<HTMLInputElement>(
        `[value='${newColor}']`);
    assert(newColorButton);
    this.setBrushColor_(newColorButton);
    newColorButton.focus();
  }

  protected isCurrentColor_(hex: string): boolean {
    return areColorsEqual(this.currentColor, hexToColor(hex));
  }

  protected getColorName_(): string {
    assert(this.currentType !== AnnotationBrushType.ERASER);
    return this.currentType === AnnotationBrushType.HIGHLIGHTER ?
        'highlighterColors' :
        'penColors';
  }

  protected getVisibleColor_(hex: string): string {
    if (this.currentType !== AnnotationBrushType.HIGHLIGHTER) {
      return hex;
    }

    // Highlighter colors are transparent, but the side panel background is
    // dark. Instead of setting the alpha value, calculate the RGB of the
    // highlighter color with transparency on a white background.
    const color = hexToColor(hex);
    color.r = blendHighlighterColorValue(color.r);
    color.g = blendHighlighterColorValue(color.g);
    color.b = blendHighlighterColorValue(color.b);

    return colorToHex(color);
  }

  protected getCurrentBrushColors_(): ColorOption[] {
    assert(this.currentType !== AnnotationBrushType.ERASER);
    return this.currentType === AnnotationBrushType.HIGHLIGHTER ?
        HIGHLIGHTER_COLORS :
        PEN_COLORS;
  }

  private setBrushColor_(colorButton: HTMLInputElement): void {
    const hex = colorButton.value;
    assert(hex);

    const newColor: Color = hexToColor(hex);
    if (areColorsEqual(this.currentColor, newColor)) {
      return;
    }

    this.currentColor = newColor;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-color-selector': InkColorSelectorElement;
  }
}

customElements.define(InkColorSelectorElement.is, InkColorSelectorElement);

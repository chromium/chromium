// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';
import './ink_size_selector.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush, Color} from '../constants.js';
import {PluginController} from '../controller.js';

import {ERASER_SIZES, HIGHLIGHTER_SIZES, PEN_SIZES} from './ink_size_selector.js';
import {getCss} from './viewer_side_panel.css.js';
import {getHtml} from './viewer_side_panel.html.js';

const NUM_OPTION_COLUMNS: number = 5;

interface ColorOption {
  name: string;
  color: string;
}

export const HIGHLIGHTER_COLORS: ColorOption[] = [
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

export const PEN_COLORS: ColorOption[] = [
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

// LINT.IfChange(HighlighterOpacity)
const HIGHLIGHTER_OPACITY: number = 0.4;
// LINT.ThenChange(//pdf/pdf_ink_brush.cc:HighlighterOpacity)

/**
 * @param hex A hex-coded color string, formatted as '#ffffff'.
 * @returns The `Color` in RGB values.
 */
function hexToColor(hex: string): Color {
  assert(/^#[0-9a-f]{6}$/.test(hex));

  return {
    r: Number.parseInt(hex.substring(1, 3), 16),
    g: Number.parseInt(hex.substring(3, 5), 16),
    b: Number.parseInt(hex.substring(5, 7), 16),
  };
}

/**
 * @param color The `Color` in RGB values.
 * @returns A hex-coded color string, formatted as '#ffffff'.
 */
function colorToHex(color: Color): string {
  const rgb = [color.r, color.g, color.b]
                  .map(value => value.toString(16).padStart(2, '0'))
                  .join('');
  return `#${rgb}`;
}

/**
 * Blends `colorValue` with highlighter opacity on a white background.
 * @param colorValue The red, green, or blue value of a color.
 * @returns The new respective red, green, or blue value of a color that has
 * been transformed using the highlighter transparency on a white background.
 */
function blendHighlighterColorValue(colorValue: number): number {
  return Math.round(
      colorValue * HIGHLIGHTER_OPACITY + 255 * (1 - HIGHLIGHTER_OPACITY));
}

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

export class ViewerSidePanelElement extends CrLitElement {
  static get is() {
    return 'viewer-side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      brushDirty_: {type: Boolean},
      currentType_: {type: String},
    };
  }

  protected currentType_: AnnotationBrushType = AnnotationBrushType.PEN;

  // Indicates the brush has changes and should be updated in
  // `this.pluginController_`.
  private brushDirty_: boolean = false;

  private brushes_: Map<AnnotationBrushType, AnnotationBrush>;
  private pluginController_: PluginController = PluginController.getInstance();

  constructor() {
    super();

    // Default brushes.
    this.brushes_ = new Map([
      [
        AnnotationBrushType.ERASER,
        {
          type: AnnotationBrushType.ERASER,
          size: ERASER_SIZES[2]!.size,
        },
      ],
      [
        AnnotationBrushType.HIGHLIGHTER,
        {
          type: AnnotationBrushType.HIGHLIGHTER,
          color: hexToColor(HIGHLIGHTER_COLORS[0]!.color),
          size: HIGHLIGHTER_SIZES[2]!.size,
        },
      ],
      [
        AnnotationBrushType.PEN,
        {
          type: AnnotationBrushType.PEN,
          color: hexToColor(PEN_COLORS[0]!.color),
          size: PEN_SIZES[2]!.size,
        },
      ],
    ]);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('brushDirty_') &&
        (this.brushDirty_ ||
         changedPrivateProperties.get('brushDirty_') === undefined)) {
      this.onBrushChanged_();
    }
  }

  protected onBrushChange_(e: CustomEvent<{type: AnnotationBrushType}>) {
    this.currentType_ = e.detail.type;
    this.brushDirty_ = true;
  }

  protected onSizeChange_(e: CustomEvent<{value: number}>) {
    this.getCurrentBrush_().size = e.detail.value;
    this.brushDirty_ = true;
  }

  protected onColorClick_(e: Event) {
    assert(this.shouldShowColorOptions_());

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
    assert(this.shouldShowColorOptions_());

    const currentColor = this.getCurrentBrush_().color;
    assert(currentColor);

    return areColorsEqual(currentColor, hexToColor(hex));
  }

  protected shouldShowColorOptions_(): boolean {
    return this.currentType_ !== AnnotationBrushType.ERASER;
  }

  protected getColorName_(): string {
    assert(this.currentType_ !== AnnotationBrushType.ERASER);
    return this.currentType_ === AnnotationBrushType.HIGHLIGHTER ?
        'highlighterColors' :
        'penColors';
  }

  protected getVisibleColor_(hex: string): string {
    if (this.currentType_ !== AnnotationBrushType.HIGHLIGHTER) {
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
    assert(this.currentType_ !== AnnotationBrushType.ERASER);
    return this.currentType_ === AnnotationBrushType.HIGHLIGHTER ?
        HIGHLIGHTER_COLORS :
        PEN_COLORS;
  }

  protected getCurrentSize_(): number {
    return this.getCurrentBrush_().size;
  }

  /**
   * When the brush changes, the new brush should be sent to
   * `this.pluginController_`.
   */
  private onBrushChanged_(): void {
    this.pluginController_.setAnnotationBrush(this.getCurrentBrush_());
    this.brushDirty_ = false;
  }

  private getCurrentBrush_(): AnnotationBrush {
    const brush = this.brushes_.get(this.currentType_);
    assert(brush);
    return brush;
  }

  private setBrushColor_(colorButton: HTMLInputElement): void {
    const currentBrush = this.getCurrentBrush_();
    const currentColor = currentBrush.color;
    assert(currentColor);

    const hex = colorButton.value;
    assert(hex);

    const newColor: Color = hexToColor(hex);
    if (areColorsEqual(currentColor, newColor)) {
      return;
    }

    currentBrush.color = newColor;
    this.brushDirty_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-side-panel': ViewerSidePanelElement;
  }
}

customElements.define(ViewerSidePanelElement.is, ViewerSidePanelElement);

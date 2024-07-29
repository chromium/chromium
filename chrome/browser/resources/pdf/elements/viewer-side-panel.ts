// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush, Color} from '../constants.js';
import {PluginController} from '../controller.js';

import {getCss} from './viewer-side-panel.css.js';
import {getHtml} from './viewer-side-panel.html.js';

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

interface SizeOption {
  name: string;
  size: number;
}

// TODO(crbug.com/341282609): Choose production size values. Add icons and
// labels.
const ERASER_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 1},
  {name: 'sizeThin', size: 2},
  {name: 'sizeExtraMedium', size: 3},
  {name: 'sizeThick', size: 6},
  {name: 'sizeExtraThick', size: 8},
];

const HIGHLIGHTER_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 4},
  {name: 'sizeThin', size: 6},
  {name: 'sizeExtraMedium', size: 8},
  {name: 'sizeThick', size: 12},
  {name: 'sizeExtraThick', size: 16},
];

const PEN_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 1},
  {name: 'sizeThin', size: 2},
  {name: 'sizeExtraMedium', size: 3},
  {name: 'sizeThick', size: 6},
  {name: 'sizeExtraThick', size: 8},
];

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
 * @returns Whether `lhs` and `rhs` have the same RGB values or not.
 */
function areColorsEqual(lhs: Color, rhs: Color): boolean {
  return lhs.r === rhs.r && lhs.g === rhs.g && lhs.b === rhs.b;
}

export interface ViewerSidePanelElement {
  $: {
    eraser: HTMLElement,
    highlighter: HTMLElement,
    pen: HTMLElement,
  };
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

  // Indicates the brush has changes and should be updated in
  // `this.pluginController_`.
  private brushDirty_: boolean = false;
  private currentType_: AnnotationBrushType = AnnotationBrushType.PEN;

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
          size: ERASER_SIZES[2].size,
        },
      ],
      [
        AnnotationBrushType.HIGHLIGHTER,
        {
          type: AnnotationBrushType.HIGHLIGHTER,
          color: hexToColor(HIGHLIGHTER_COLORS[0].color),
          size: HIGHLIGHTER_SIZES[2].size,
        },
      ],
      [
        AnnotationBrushType.PEN,
        {
          type: AnnotationBrushType.PEN,
          color: hexToColor(PEN_COLORS[0].color),
          size: PEN_SIZES[2].size,
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

  protected onBrushClick_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const newType = targetElement.dataset['brush'] as AnnotationBrushType;
    if (this.currentType_ === newType) {
      return;
    }

    this.currentType_ = newType;
    this.brushDirty_ = true;
  }

  protected onSizeClick_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const size = Number(targetElement.dataset['size']);

    const currentBrush = this.getCurrentBrush_();
    if (currentBrush.size === size) {
      return;
    }

    currentBrush.size = size;
    this.brushDirty_ = true;
  }

  protected onColorClick_(e: Event) {
    assert(this.shouldShowColorOptions_());

    const currentBrush = this.getCurrentBrush_();
    const currentColor = currentBrush.color;
    assert(currentColor);

    const targetElement = e.currentTarget as HTMLInputElement;
    const hex = targetElement.value;
    assert(hex);

    const newColor: Color = hexToColor(hex);
    if (areColorsEqual(currentColor, newColor)) {
      return;
    }

    currentBrush.color = newColor;
    this.brushDirty_ = true;
  }

  protected isCurrentType_(type: AnnotationBrushType): boolean {
    return this.currentType_ === type;
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

  protected getCurrentBrushSizes_(): SizeOption[] {
    switch (this.currentType_) {
      case AnnotationBrushType.ERASER:
        return ERASER_SIZES;
      case AnnotationBrushType.HIGHLIGHTER:
        return HIGHLIGHTER_SIZES;
      case AnnotationBrushType.PEN:
        return PEN_SIZES;
    }
  }

  protected getCurrentBrushColors_(): ColorOption[] {
    assert(this.currentType_ !== AnnotationBrushType.ERASER);
    return this.currentType_ === AnnotationBrushType.HIGHLIGHTER ?
        HIGHLIGHTER_COLORS :
        PEN_COLORS;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-side-panel': ViewerSidePanelElement;
  }
}

customElements.define(ViewerSidePanelElement.is, ViewerSidePanelElement);

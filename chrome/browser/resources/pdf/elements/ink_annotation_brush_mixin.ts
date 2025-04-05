// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush, Color} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import type {ColorOption} from './ink_color_selector.js';
import {PEN_SIZES} from './ink_size_selector.js';

type Constructor<T> = new (...args: any[]) => T;

export const HIGHLIGHTER_COLORS: ColorOption[] = [
  // LINT.IfChange(HighlighterColors)
  // Row 1:
  {label: 'ink2BrushColorLightRed', color: '#f28b82', blended: true},
  {label: 'ink2BrushColorLightYellow', color: '#fdd663', blended: true},
  {label: 'annotationColorLightGreen', color: '#34a853', blended: true},
  {label: 'annotationColorLightBlue', color: '#4285f4', blended: true},
  {label: 'annotationColorLightOrange', color: '#ffae80', blended: true},
  // Row 2:
  {label: 'annotationColorRed', color: '#d93025', blended: true},
  {label: 'annotationColorYellow', color: '#ddf300', blended: true},
  {label: 'annotationColorGreen', color: '#25e387', blended: true},
  {label: 'annotationColorBlue', color: '#5379ff', blended: true},
  {label: 'annotationColorOrange', color: '#ff630c', blended: true},
  // LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:HighlighterColors)
];

export const PEN_COLORS: ColorOption[] = [
  // LINT.IfChange(PenColors)
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
  {label: 'ink2BrushColorBlue1', color: '#8ab4f8', blended: false},
  {label: 'ink2BrushColorTan1', color: '#eec9ae', blended: false},
  // Row 3:
  {label: 'ink2BrushColorRed2', color: '#ea4335', blended: false},
  {label: 'ink2BrushColorYellow2', color: '#fbbc04', blended: false},
  {label: 'ink2BrushColorGreen2', color: '#34a853', blended: false},
  {label: 'ink2BrushColorBlue2', color: '#4285f4', blended: false},
  {label: 'ink2BrushColorTan2', color: '#e2a185', blended: false},
  // Row 4:
  {label: 'ink2BrushColorRed3', color: '#c5221f', blended: false},
  {label: 'ink2BrushColorYellow3', color: '#f29900', blended: false},
  {label: 'ink2BrushColorGreen3', color: '#188038', blended: false},
  {label: 'ink2BrushColorBlue3', color: '#1967d2', blended: false},
  {label: 'ink2BrushColorTan3', color: '#885945', blended: false},
  // LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:PenColors)
];

export const InkAnnotationBrushMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<InkAnnotationBrushMixinInterface> => {
  class InkAnnotationBrushMixin extends superClass implements
      InkAnnotationBrushMixinInterface {
    static get properties() {
      return {
        currentColor: {type: Object},
        currentSize: {type: Number},
        currentType: {type: String},
      };
    }

    private tracker_: EventTracker = new EventTracker();
    accessor currentColor: Color = {r: 0, g: 0, b: 0};
    accessor currentSize: number = PEN_SIZES[0]!.size;
    accessor currentType: AnnotationBrushType = AnnotationBrushType.PEN;

    constructor(..._args: any[]) {
      super();
      if (Ink2Manager.getInstance().isInitializationComplete()) {
        this.onBrushChanged_(Ink2Manager.getInstance().getCurrentBrush());
      }
    }

    override connectedCallback() {
      super.connectedCallback();
      this.tracker_.add(
          Ink2Manager.getInstance(), 'brush-changed',
          (e: Event) =>
              this.onBrushChanged_((e as CustomEvent<AnnotationBrush>).detail));
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      this.tracker_.removeAll();
    }

    availableBrushColors(): ColorOption[] {
      // This mixin's clients use conditional rendering to avoid invoking this
      // method when the ERASER is selected.
      assert(this.currentType !== AnnotationBrushType.ERASER);
      return this.currentType === AnnotationBrushType.PEN ? PEN_COLORS :
                                                            HIGHLIGHTER_COLORS;
    }

    onCurrentColorChanged(e: CustomEvent<{value: Color}>) {
      // Avoid poking the plugin if the value hasn't actually changed.
      const newColor = e.detail.value;
      if (newColor.r !== this.currentColor.r ||
          newColor.b !== this.currentColor.b ||
          newColor.g !== this.currentColor.g) {
        Ink2Manager.getInstance().setBrushColor(newColor);
      }
    }

    onCurrentSizeChanged(e: CustomEvent<{value: number}>) {
      if (e.detail.value !== this.currentSize) {
        Ink2Manager.getInstance().setBrushSize(e.detail.value);
      }
    }

    onCurrentTypeChanged(e: CustomEvent<{value: AnnotationBrushType}>) {
      if (e.detail.value !== this.currentType) {
        Ink2Manager.getInstance().setBrushType(e.detail.value);
      }
    }

    private onBrushChanged_(brush: AnnotationBrush) {
      this.currentType = brush.type;
      // Detail and size may be undefined if the brush was set to eraser.
      if (brush.size !== undefined) {
        this.currentSize = brush.size;
      }
      if (brush.color !== undefined) {
        this.currentColor = brush.color;
      }
    }
  }
  return InkAnnotationBrushMixin;
};

export interface InkAnnotationBrushMixinInterface {
  currentColor: Color;
  currentSize: number;
  currentType: AnnotationBrushType;
  availableBrushColors(): ColorOption[];
  onCurrentColorChanged(e: CustomEvent<{value: Color}>): void;
  onCurrentSizeChanged(e: CustomEvent<{value: number}>): void;
  onCurrentTypeChanged(e: CustomEvent<{value: AnnotationBrushType}>): void;
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush, Color} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import {PEN_SIZES} from './ink_size_selector.js';

type Constructor<T> = new (...args: any[]) => T;

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
    currentColor: Color = {r: 0, g: 0, b: 0};
    currentSize: number = PEN_SIZES[0]!.size;
    currentType: AnnotationBrushType = AnnotationBrushType.PEN;

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
  onCurrentColorChanged(e: CustomEvent<{value: Color}>): void;
  onCurrentSizeChanged(e: CustomEvent<{value: number}>): void;
  onCurrentTypeChanged(e: CustomEvent<{value: AnnotationBrushType}>): void;
}

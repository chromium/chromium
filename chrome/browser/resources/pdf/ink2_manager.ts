// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {AnnotationBrush, Color} from './constants.js';
import {AnnotationBrushType} from './constants.js';
import {PluginController} from './controller.js';

export class Ink2Manager extends EventTarget {
  private brush_: AnnotationBrush = {type: AnnotationBrushType.PEN};
  private brushResolver_: PromiseResolver<void>|null = null;
  private pluginController_: PluginController = PluginController.getInstance();

  isInitializationStarted(): boolean {
    return this.brushResolver_ !== null;
  }

  isInitializationComplete(): boolean {
    return this.isInitializationStarted() && this.brushResolver_!.isFulfilled;
  }

  getCurrentBrush(): AnnotationBrush {
    assert(this.isInitializationComplete());
    return this.brush_;
  }

  initializeBrush(): Promise<void> {
    assert(this.brushResolver_ === null);
    this.brushResolver_ = new PromiseResolver();
    this.pluginController_.getAnnotationBrush().then(defaultBrushMessage => {
      this.setAnnotationBrush_(defaultBrushMessage.data);
      assert(this.brushResolver_);
      this.brushResolver_.resolve();
    });
    return this.brushResolver_.promise;
  }

  setBrushColor(color: Color) {
    assert(this.brush_.type !== AnnotationBrushType.ERASER);
    if (this.brush_.color === color) {
      return;
    }

    this.brush_.color = color;
    this.fireBrushChanged_();
    this.setAnnotationBrushInPlugin_();
  }

  setBrushSize(size: number) {
    if (this.brush_.size === size) {
      return;
    }

    this.brush_.size = size;
    this.fireBrushChanged_();
    this.setAnnotationBrushInPlugin_();
  }

  async setBrushType(type: AnnotationBrushType): Promise<void> {
    if (this.brush_.type === type) {
      return;
    }

    const brushMessage = await this.pluginController_.getAnnotationBrush(type);
    this.setAnnotationBrush_(brushMessage.data);
    this.setAnnotationBrushInPlugin_();
  }

  /**
   * Sets the current brush properties to the values in `brush`.
   */
  private setAnnotationBrush_(brush: AnnotationBrush): void {
    this.brush_ = brush;
    this.fireBrushChanged_();
  }

  /**
   * Sets the annotation brush in the plugin with the current brush parameters.
   */
  private setAnnotationBrushInPlugin_(): void {
    this.pluginController_.setAnnotationBrush(this.brush_);
  }

  private fireBrushChanged_() {
    this.dispatchEvent(new CustomEvent('brush-changed', {detail: this.brush_}));
  }

  static getInstance(): Ink2Manager {
    return instance || (instance = new Ink2Manager());
  }

  static setInstance(obj: Ink2Manager) {
    instance = obj;
  }
}

let instance: (Ink2Manager|null) = null;

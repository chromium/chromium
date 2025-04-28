// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {AnnotationBrush, Color, TextAttributes, TextBoxRect, TextStyles} from './constants.js';
import {AnnotationBrushType, TextAlignment, TextStyle} from './constants.js';
import type {MessageData} from './controller.js';
import {PluginController, PluginControllerEventType} from './controller.js';
import type {Viewport} from './viewport.js';

export interface ViewportParams {
  pageX: number;
  pageY: number;
  zoom: number;
}

export class Ink2Manager extends EventTarget {
  private brush_: AnnotationBrush = {type: AnnotationBrushType.PEN};
  private attributes_: TextAttributes = {
    typeface: '',
    size: 12,
    color: {r: 0, g: 0, b: 0},
    alignment: TextAlignment.LEFT,
    styles: {
      [TextStyle.BOLD]: false,
      [TextStyle.ITALIC]: false,
      [TextStyle.UNDERLINE]: false,
      [TextStyle.STRIKETHROUGH]: false,
    },
  };
  private brushResolver_: PromiseResolver<void>|null = null;
  private fontNamesResolver_: PromiseResolver<string[]>|null = null;
  private pluginController_: PluginController = PluginController.getInstance();
  private viewport_: Viewport|null = null;
  private viewportParams_: ViewportParams = {pageX: 0, pageY: 0, zoom: 1.0};

  constructor() {
    super();
    this.pluginController_.getEventTarget().addEventListener(
        PluginControllerEventType.PLUGIN_MESSAGE,
        (e: Event) => this.handlePluginMessage_(e as CustomEvent<MessageData>));
  }

  setViewport(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  private handlePluginMessage_(e: CustomEvent<MessageData>) {
    const data = e.detail;
    if (data.type.toString() === 'updateTextAnnotTextBoxRect') {
      const detail = data as unknown as TextBoxRect;
      this.dispatchEvent(new CustomEvent('update-text-box', {detail}));
    }
  }

  getViewportParams(): ViewportParams {
    return this.viewportParams_;
  }

  viewportChanged() {
    assert(this.viewport_, 'Must call setViewport() before viewportChanged()');
    const visiblePage = this.viewport_.getMostVisiblePage();
    const visiblePageDimensions = this.viewport_.getPageScreenRect(visiblePage);
    const zoom = this.viewport_.getZoom();
    if (visiblePageDimensions.x === this.viewportParams_.pageX &&
        visiblePageDimensions.y === this.viewportParams_.pageY &&
        zoom === this.viewportParams_.zoom) {
      // Early return to avoid firing unnecessary events.
      return;
    }

    this.viewportParams_ = {
      pageX: visiblePageDimensions.x,
      pageY: visiblePageDimensions.y,
      zoom,
    };
    this.dispatchEvent(
        new CustomEvent('viewport-changed', {detail: this.viewportParams_}));
  }

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

  getCurrentTextAttributes(): TextAttributes {
    return this.attributes_;
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

  getTextAnnotationFontNames(): Promise<string[]> {
    if (this.fontNamesResolver_ === null) {
      this.fontNamesResolver_ = new PromiseResolver();
      this.pluginController_.getTextAnnotFontNames().then(fontsMessage => {
        assert(this.fontNamesResolver_);
        this.fontNamesResolver_.resolve(fontsMessage.data);
        assert(fontsMessage.data.length > 0);
        this.setTextTypeface(fontsMessage.data[0]!);
      });
    }
    return this.fontNamesResolver_.promise;
  }

  setTextTypeface(typeface: string) {
    if (this.attributes_.typeface === typeface) {
      return;
    }

    this.attributes_.typeface = typeface;
    this.updatedText_();
  }

  setTextSize(size: number) {
    if (this.attributes_.size === size) {
      return;
    }

    this.attributes_.size = size;
    this.updatedText_();
  }

  setTextColor(color: Color) {
    if (this.attributes_.color.r === color.r &&
        this.attributes_.color.g === color.g &&
        this.attributes_.color.b === color.b) {
      return;
    }

    this.attributes_.color = color;
    this.updatedText_();
  }

  setTextAlignment(alignment: TextAlignment) {
    if (this.attributes_.alignment === alignment) {
      return;
    }

    this.attributes_.alignment = alignment;
    this.updatedText_();
  }

  setTextStyles(styles: TextStyles) {
    if (this.attributes_.styles[TextStyle.BOLD] === styles[TextStyle.BOLD] &&
        this.attributes_.styles[TextStyle.ITALIC] ===
            styles[TextStyle.ITALIC] &&
        this.attributes_.styles[TextStyle.UNDERLINE] ===
            styles[TextStyle.UNDERLINE] &&
        this.attributes_.styles[TextStyle.STRIKETHROUGH] ===
            styles[TextStyle.STRIKETHROUGH]) {
      return;
    }

    this.attributes_.styles = styles;
    this.updatedText_();
  }

  setTextBoxRect(update: TextBoxRect) {
    this.pluginController_.setTextAnnotTextBoxRect(update);
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

  private updatedText_(): void {
    this.setAnnotationTextInPlugin_();
    this.fireFontChanged_();
  }

  private setAnnotationTextInPlugin_(): void {
    this.pluginController_.setTextAnnotationFont({
      typeface: this.attributes_.typeface,
      fontSize: this.attributes_.size,
      alignment: this.attributes_.alignment,
      style: this.attributes_.styles,
      color: this.attributes_.color,
    });
  }

  private fireFontChanged_() {
    this.dispatchEvent(
        new CustomEvent('attributes-changed', {detail: this.attributes_}));
  }

  static getInstance(): Ink2Manager {
    return instance || (instance = new Ink2Manager());
  }

  static setInstance(obj: Ink2Manager) {
    instance = obj;
  }
}

let instance: (Ink2Manager|null) = null;

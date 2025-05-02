// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {AnnotationBrush, Color, Point, TextAnnotation, TextAttributes, TextBoxRect, TextStyles} from './constants.js';
import {AnnotationBrushType, TextAlignment, TextStyle} from './constants.js';
import {PluginController} from './controller.js';
import type {Viewport} from './viewport.js';

export interface ViewportParams {
  pageX: number;
  pageY: number;
  zoom: number;
}

export interface TextBoxInit {
  annotation: TextAnnotation;
  pageCoordinates: Point;
}

export const DEFAULT_TEXTBOX_WIDTH: number = 200;
export const DEFAULT_TEXTBOX_HEIGHT: number = 100;

export function colorsEqual(color1: Color, color2: Color): boolean {
  return color1.r === color2.r && color1.g === color2.g &&
      color1.b === color2.b;
}

export function stylesEqual(style1: TextStyles, style2: TextStyles): boolean {
  return style1.bold === style2.bold && style1.italic === style2.italic &&
      style1.underline === style2.underline &&
      style1.strikethrough === style2.strikethrough;
}

export class Ink2Manager extends EventTarget {
  private brush_: AnnotationBrush = {type: AnnotationBrushType.PEN};
  // Map from page numbers to annotations on that page.
  // The annotations on each page are stored in a map from id to TextAnnotation.
  private annotations_: Map<number, Map<number, TextAnnotation>> = new Map();
  // The attributes selected by the user for new annotations.
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
  // Holds text attributes pre-populated from an existing annotation that the
  // user is editing. Null if the user is not editing an annotation or is
  // creating a new annotation using |attributes_|.
  private existingAnnotationAttributes_: TextAttributes|null = null;
  private fontNamesResolver_: PromiseResolver<string[]>|null = null;
  private pageNumber_: number = -1;
  private pluginController_: PluginController = PluginController.getInstance();
  private viewport_: Viewport|null = null;
  private viewportParams_: ViewportParams = {pageX: 0, pageY: 0, zoom: 1.0};
  private nextAnnotationId_: number = 0;

  setViewport(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  // Initialize a text annotation at `location` in screen coordinates.
  // No-op if there is no PDF page at `location`.
  initializeTextAnnotation(location: Point) {
    assert(this.viewport_);
    const page = this.viewport_.getPageAtPoint(location);
    if (page === -1) {
      return;
    }

    const zoom = this.viewport_.getZoom();
    const pageDimensions = this.viewport_.getPageScreenRect(page);
    // Is the click in an existing box?
    let existing = null;
    // Get the annotations for the current page.
    const annotationsMap = this.annotations_.get(page);
    const annotations =
        annotationsMap ? Array.from(annotationsMap.values()) : [];
    for (const annotation of annotations) {
      // Convert box to screen coordinates.
      const x = annotation.textBoxRect.locationX * zoom + pageDimensions.x;
      const width = annotation.textBoxRect.width * zoom;
      const y = annotation.textBoxRect.locationY * zoom + pageDimensions.y;
      const height = annotation.textBoxRect.height * zoom;
      if (location.x >= x && location.x <= (x + width) && location.y >= y &&
          location.y <= (y + height)) {
        // Don't update the original. Create a new object and update its
        // rectangle to use the computed screen coordinates.
        existing = structuredClone(annotation);
        existing.textBoxRect = {height, locationX: x, locationY: y, width};
        break;
      }
    }

    this.pageNumber_ = page;
    const annotation = existing ? existing : {
      text: '',
      id: this.nextAnnotationId_,
      pageNumber: page,
      textAttributes: structuredClone(this.attributes_),
      textBoxRect: {
        height: DEFAULT_TEXTBOX_HEIGHT,
        locationX: location.x,
        locationY: location.y,
        width: DEFAULT_TEXTBOX_WIDTH,
      },
    };

    if (existing) {
      this.pluginController_.startTextAnnotation(existing.id);
      this.existingAnnotationAttributes_ = annotation.textAttributes;
    } else {
      this.nextAnnotationId_++;
      this.existingAnnotationAttributes_ = null;
    }

    this.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation,
        pageCoordinates: {x: pageDimensions.x, y: pageDimensions.y},
      },
    }));

    // Notify other listeners of any changes to the viewport and/or attributes,
    // since these may change with the annotation.
    this.viewportChanged();
    this.fireAttributesChanged_();
  }

  getViewportParams(): ViewportParams {
    return this.viewportParams_;
  }

  viewportChanged() {
    assert(this.viewport_, 'Must call setViewport() before viewportChanged()');
    const zoom = this.viewport_.getZoom();
    const page = this.pageNumber_ !== -1 ? this.pageNumber_ :
                                           this.viewport_.getMostVisiblePage();
    const visiblePageDimensions = this.viewport_.getPageScreenRect(page);
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
    return this.existingAnnotationAttributes_ ?
        this.existingAnnotationAttributes_ :
        this.attributes_;
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
    const current = this.getCurrentTextAttributes();
    if (current.typeface === typeface) {
      return;
    }

    current.typeface = typeface;
    this.fireAttributesChanged_();
  }

  setTextSize(size: number) {
    const current = this.getCurrentTextAttributes();
    if (current.size === size) {
      return;
    }

    current.size = size;
    this.fireAttributesChanged_();
  }

  setTextColor(color: Color) {
    const current = this.getCurrentTextAttributes();
    if (colorsEqual(current.color, color)) {
      return;
    }

    current.color = color;
    this.fireAttributesChanged_();
  }

  setTextAlignment(alignment: TextAlignment) {
    const current = this.getCurrentTextAttributes();
    if (current.alignment === alignment) {
      return;
    }

    current.alignment = alignment;
    this.fireAttributesChanged_();
  }

  setTextStyles(styles: TextStyles) {
    const current = this.getCurrentTextAttributes();
    if (stylesEqual(current.styles, styles)) {
      return;
    }

    current.styles = styles;
    this.fireAttributesChanged_();
  }

  private screenToPageCoordinates_(pageNumber: number, screenRect: TextBoxRect):
      TextBoxRect {
    assert(this.viewport_);
    const pageDimensions = this.viewport_.getPageScreenRect(pageNumber);
    const zoom = this.viewport_.getZoom();
    return {
      height: screenRect.height / zoom,
      locationX: (screenRect.locationX - pageDimensions.x) / zoom,
      locationY: (screenRect.locationY - pageDimensions.y) / zoom,
      width: screenRect.width / zoom,
    };
  }

  /**
   * Updates the stored annotation and notifies the plugin of the new or
   * modified annotation.
   */
  commitTextAnnotation(annotation: TextAnnotation) {
    annotation.textBoxRect = this.screenToPageCoordinates_(
        annotation.pageNumber, annotation.textBoxRect);

    let pageAnnotations = this.annotations_.get(annotation.pageNumber);
    if (!pageAnnotations) {
      // Adding a new annotation, on a page that doesn't have any existing ones.
      // Create and add the new map.
      pageAnnotations = new Map();
      this.annotations_.set(annotation.pageNumber, pageAnnotations);
    }

    if (pageAnnotations.has(annotation.id) && annotation.text === '') {
      // Delete an existing annotation.
      pageAnnotations.delete(annotation.id);
    } else {
      pageAnnotations.set(annotation.id, annotation);
    }
    this.pluginController_.finishTextAnnotation(annotation);
    this.existingAnnotationAttributes_ = null;
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

  private fireAttributesChanged_() {
    this.dispatchEvent(new CustomEvent(
        'attributes-changed',
        {detail: structuredClone(this.getCurrentTextAttributes())}));
  }

  static getInstance(): Ink2Manager {
    return instance || (instance = new Ink2Manager());
  }

  static setInstance(obj: Ink2Manager) {
    instance = obj;
  }
}

let instance: (Ink2Manager|null) = null;

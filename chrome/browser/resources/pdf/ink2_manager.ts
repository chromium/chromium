// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {AnnotationBrush, Color, Point, TextAnnotation, TextAttributes, TextBoxRect, TextStyles} from './constants.js';
import {AnnotationBrushType, TextAlignment, TextStyle, TextTypeface} from './constants.js';
import {PluginController, PluginControllerEventType} from './controller.js';
import type {Viewport, ViewportRect} from './viewport.js';

export interface ViewportParams {
  clockwiseRotations: number;
  pageDimensions: ViewportRect;
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
  return style1.bold === style2.bold && style1.italic === style2.italic;
}

/**
 * Converts `rect` from `oldRotations` clockwise rotations to `newRotations`
 * clockwise rotations. `newPageWidth` should be the page width in
 * `newRotations` coordinates, and `newPageHeight` should be the page height in
 * `newRotations` coordinates.
 */
export function convertRotatedCoordinates(
    rect: TextBoxRect, oldRotations: number, newRotations: number,
    newPageWidth: number, newPageHeight: number): TextBoxRect {
  const pageWidthNR = newRotations % 2 === 0 ? newPageWidth : newPageHeight;
  const pageHeightNR = newRotations % 2 === 0 ? newPageHeight : newPageWidth;
  const nonRotated: TextBoxRect = {
    locationX: rect.locationX,
    locationY: rect.locationY,
    width: oldRotations % 2 === 0 ? rect.width : rect.height,
    height: oldRotations % 2 === 0 ? rect.height : rect.width,
  };
  switch (oldRotations % 4) {
    case 0:
      // Already populated correctly.
      break;
    case 1:
      nonRotated.locationX = rect.locationY;
      nonRotated.locationY = pageHeightNR - rect.locationX - rect.width;
      break;
    case 2:
      nonRotated.locationX = pageWidthNR - rect.locationX - rect.width;
      nonRotated.locationY = pageHeightNR - rect.locationY - rect.height;
      break;
    case 3:
      nonRotated.locationX = pageWidthNR - rect.locationY - rect.height;
      nonRotated.locationY = rect.locationX;
      break;
    default:
      assertNotReached();
  }

  const newRotated = {
    locationX: nonRotated.locationX,
    locationY: nonRotated.locationY,
    width: newRotations % 2 === 0 ? nonRotated.width : nonRotated.height,
    height: newRotations % 2 === 0 ? nonRotated.height : nonRotated.width,
  };
  switch (newRotations % 4) {
    case 0:
      break;
    case 1:
      newRotated.locationX =
          pageHeightNR - nonRotated.locationY - nonRotated.height;
      newRotated.locationY = nonRotated.locationX;
      break;
    case 2:
      newRotated.locationX =
          pageWidthNR - nonRotated.locationX - nonRotated.width;
      newRotated.locationY =
          pageHeightNR - nonRotated.locationY - nonRotated.height;
      break;
    case 3:
      newRotated.locationX = nonRotated.locationY;
      newRotated.locationY =
          pageWidthNR - nonRotated.locationX - nonRotated.width;
      break;
    default:
      assertNotReached();
  }
  return newRotated;
}

export class Ink2Manager extends EventTarget {
  private brush_: AnnotationBrush = {type: AnnotationBrushType.PEN};
  // Map from page numbers to annotations on that page.
  // The annotations on each page are stored in a map from id to TextAnnotation.
  private annotations_: Map<number, Map<number, TextAnnotation>> = new Map();
  // The attributes selected by the user for new annotations.
  private attributes_: TextAttributes = {
    typeface: TextTypeface.SANS_SERIF,
    size: 12,
    color: {r: 0, g: 0, b: 0},
    alignment: TextAlignment.LEFT,
    styles: {
      [TextStyle.BOLD]: false,
      [TextStyle.ITALIC]: false,
    },
  };
  private brushResolver_: PromiseResolver<void>|null = null;
  // Holds text attributes pre-populated from an existing annotation that the
  // user is editing. Null if the user is not editing an annotation or is
  // creating a new annotation using |attributes_|.
  private existingAnnotationAttributes_: TextAttributes|null = null;
  private pageNumber_: number = -1;
  private pluginController_: PluginController = PluginController.getInstance();
  private textResolver_: PromiseResolver<void>|null = null;
  private viewport_: Viewport|null = null;
  private viewportParams_: ViewportParams = {
    clockwiseRotations: 0,
    pageDimensions: {x: 0, y: 0, width: 0, height: 0},
    zoom: 1.0,
  };
  private nextAnnotationId_: number = 0;

  setViewport(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  resetAnnotationIdForTest() {
    this.nextAnnotationId_ = 0;
  }

  private isClickOnScrollbar_(location: Point): boolean {
    assert(this.viewport_);
    const hasScrollbars = this.viewport_.documentHasScrollbars();
    if (hasScrollbars.vertical &&
            (isRTL() && location.x <= this.viewport_.scrollbarWidth) ||
        (!isRTL() &&
         location.x >=
             (this.viewport_.size.width - this.viewport_.scrollbarWidth))) {
      return true;
    }
    return hasScrollbars.horizontal &&
        location.y >=
        (this.viewport_.size.height - this.viewport_.scrollbarWidth);
  }

  // Initialize a text annotation at `location` in screen coordinates.
  // No-op if there is no PDF page at `location`.
  // If location is not provided, creates the annotation at the center of
  // the visible portion of the most visible page.
  initializeTextAnnotation(location?: Point) {
    assert(this.isTextInitializationComplete());
    assert(this.viewport_);

    let page = -1;
    if (!location) {
      page = this.viewport_.getMostVisiblePage();
    } else if (!this.isClickOnScrollbar_(location)) {
      // Only actually compute the page if the click isn't on a scrollbar.
      page = this.viewport_.getPageAtPoint(location);
    }

    if (page === -1) {
      // In any case where we ignore the click, blur the textbox. Otherwise,
      // the textarea will remain in focus and will continue handling all
      // keyboard events, which is inconsistent with how clicking on other parts
      // of the UI (e.g. controls) work.
      this.dispatchEvent(new CustomEvent('blur-text-box'));
      return;
    }

    const pageDimensions = this.viewport_.getPageScreenRect(page);
    // Set location to the middle of the visible portion of the page.
    if (!location) {
      const minX = Math.max(pageDimensions.x, 0);
      const minY = Math.max(pageDimensions.y, 0);
      const maxX = Math.min(
          pageDimensions.x + pageDimensions.width, this.viewport_.size.width);
      const maxY = Math.min(
          pageDimensions.y + pageDimensions.height, this.viewport_.size.height);
      location = {
        x: Math.max(0, (minX + maxX) / 2 - DEFAULT_TEXTBOX_WIDTH / 2),
        y: Math.max(0, (minY + maxY) / 2 - DEFAULT_TEXTBOX_HEIGHT / 2),
      };
    }

    // Is the click in an existing box?
    let existing = null;
    // Get the annotations for the current page.
    const annotationsMap = this.annotations_.get(page);
    const annotations =
        annotationsMap ? Array.from(annotationsMap.values()) : [];
    for (const annotation of annotations) {
      // Convert box to screen coordinates.
      const screenBox =
          this.pageToScreenCoordinates_(page, annotation.textBoxRect);
      if (location.x >= screenBox.locationX &&
          location.x <= (screenBox.locationX + screenBox.width) &&
          location.y >= screenBox.locationY &&
          location.y <= (screenBox.locationY + screenBox.height)) {
        // Don't update the original. Create a new object and update its
        // rectangle to use the computed screen coordinates.
        existing = structuredClone(annotation);
        existing.textBoxRect = screenBox;
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
      textOrientation: (4 - this.viewport_.getClockwiseRotations()) % 4,
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
    const pageDimensions = this.viewport_.getPageScreenRect(page);
    const rotations = this.viewport_.getClockwiseRotations();
    if (rotations === this.viewportParams_.clockwiseRotations &&
        pageDimensions.x === this.viewportParams_.pageDimensions.x &&
        pageDimensions.y === this.viewportParams_.pageDimensions.y &&
        pageDimensions.width === this.viewportParams_.pageDimensions.width &&
        pageDimensions.height === this.viewportParams_.pageDimensions.height &&
        zoom === this.viewportParams_.zoom) {
      // Early return to avoid firing unnecessary events.
      return;
    }

    this.viewportParams_ = {
      clockwiseRotations: rotations,
      pageDimensions: pageDimensions,
      zoom,
    };
    this.dispatchEvent(
        new CustomEvent('viewport-changed', {detail: this.viewportParams_}));
  }

  isInitializationStarted(): boolean {
    return this.brushResolver_ !== null;
  }

  isTextInitializationComplete(): boolean {
    return this.textResolver_ !== null && this.textResolver_.isFulfilled;
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

  initializeTextAnnotations(): Promise<void> {
    if (this.textResolver_) {
      return this.textResolver_.promise;
    }

    this.textResolver_ = new PromiseResolver();
    this.pluginController_.getAllTextAnnotations().then(message => {
      message.annotations.forEach(annotation => {
        let pageMap = this.annotations_.get(annotation.pageNumber);
        if (!pageMap) {
          pageMap = new Map();
          this.annotations_.set(annotation.pageNumber, pageMap);
        }
        pageMap.set(annotation.id, annotation);
        if (annotation.id > this.nextAnnotationId_) {
          this.nextAnnotationId_ = annotation.id + 1;
        }
      });
      this.textResolver_!.resolve();
    });
    return this.textResolver_.promise;
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

  setTextTypeface(typeface: TextTypeface) {
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

  private pageToScreenCoordinates_(pageNumber: number, pageRect: TextBoxRect):
      TextBoxRect {
    assert(this.viewport_);
    const pageDimensions = this.viewport_.getPageScreenRect(pageNumber);
    const zoom = this.viewport_.getZoom();

    // Apply zoom.
    const zoomed = {
      locationX: pageRect.locationX * zoom,
      locationY: pageRect.locationY * zoom,
      width: pageRect.width * zoom,
      height: pageRect.height * zoom,
    };

    // Apply rotation
    const rotated = convertRotatedCoordinates(
        zoomed, 0, this.viewport_.getClockwiseRotations(), pageDimensions.width,
        pageDimensions.height);

    // Apply offsets.
    return {
      locationX: rotated.locationX + pageDimensions.x,
      locationY: rotated.locationY + pageDimensions.y,
      height: rotated.height,
      width: rotated.width,
    };
  }

  private screenToPageCoordinates_(pageNumber: number, screenRect: TextBoxRect):
      TextBoxRect {
    assert(this.viewport_);
    const zoom = this.viewport_.getZoom();
    const pageDimensions = this.viewport_.getPageScreenRect(pageNumber);

    // Undo offset
    const noOffset = {
      locationX: screenRect.locationX - pageDimensions.x,
      locationY: screenRect.locationY - pageDimensions.y,
      width: screenRect.width,
      height: screenRect.height,
    };

    // Undo rotation
    const rotations = this.viewport_.getClockwiseRotations();
    // Need to pass the width and height for the new number of desired rotations
    // (0 in this case) to convertRotatedCoordinates().
    const pageWidth =
        rotations % 2 === 0 ? pageDimensions.width : pageDimensions.height;
    const pageHeight =
        rotations % 2 === 0 ? pageDimensions.height : pageDimensions.width;
    const noRotation = convertRotatedCoordinates(
        noOffset, rotations, 0, pageWidth, pageHeight);

    // Undo zoom.
    return {
      height: noRotation.height / zoom,
      locationX: noRotation.locationX / zoom,
      locationY: noRotation.locationY / zoom,
      width: noRotation.width / zoom,
    };
  }

  /**
   * Updates the stored annotation and notifies the plugin of the new or
   * modified annotation.
   */
  commitTextAnnotation(annotation: TextAnnotation, edited: boolean) {
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

    // Using PluginController's event target to dispatch this event, even
    // though it originates here, because PluginController dispatches this
    // event for normal Ink strokes and this way clients only need to listen
    // on one instance.
    this.pluginController_.getEventTarget().dispatchEvent(new CustomEvent(
        PluginControllerEventType.FINISH_INK_STROKE, {detail: edited}));
  }

  textBoxFocused(textBoxRect: TextBoxRect) {
    assert(this.viewport_);
    const viewportPosition = this.viewport_.position;
    const viewportSize = this.viewport_.size;

    let scrollX: number|undefined;
    let scrollY: number|undefined;
    if (textBoxRect.locationX < 0 ||
        textBoxRect.locationX + textBoxRect.width > viewportSize.width) {
      // Adjusting by 10% of viewport, rather than putting the text box on the
      // exact edge of the viewport.
      scrollX = viewportPosition.x + textBoxRect.locationX -
          Math.floor(viewportSize.width / 10);
    }

    if (textBoxRect.locationY < 0 ||
        textBoxRect.locationY + textBoxRect.height > viewportSize.height) {
      scrollY = viewportPosition.y + textBoxRect.locationY -
          Math.floor(viewportSize.height / 10);
    }

    if (scrollX !== undefined || scrollY !== undefined) {
      this.viewport_.scrollTo({x: scrollX, y: scrollY});
    }
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import type {AnnotationBrush, Color, Point, TextAnnotation, TextAnnotationMessageData, TextAttributes, TextBoxRect, TextStyles} from './constants.js';
import {AnnotationBrushType, TextAlignment, TextAnnotationSource, TextStyle, TextTypeface} from './constants.js';
import {PluginController, PluginControllerEventType} from './controller.js';
import {pageToScreenCoordinates, screenToPageCoordinates} from './ink_text_annotation_utils.js';
import {colorsEqual} from './pdf_viewer_utils.js';
import {UndoRedoStack} from './undo_redo_stack.js';
import type {Viewport, ViewportRect} from './viewport.js';

export interface TextBoxInit {
  annotation: TextAnnotation;
  pageDimensions: ViewportRect;
  isPaste?: boolean;
}

export const DEFAULT_TEXTBOX_WIDTH: number = 222;

// Blink crashes when rendering a textarea that is too small (<24px wide).
// This value is held constant regardless of zoom due to the rendering issue.
export const MIN_TEXTBOX_SIZE_PX = 24;

export function stylesEqual(style1: TextStyles, style2: TextStyles): boolean {
  return style1.bold === style2.bold && style1.italic === style2.italic &&
      style1.strikethrough === style2.strikethrough;
}

function getClampedLocation(
    location: Point, width: number, height: number,
    pageDimensions: ViewportRect): Point {
  const maxX = pageDimensions.x + pageDimensions.width - width;
  const maxY = pageDimensions.y + pageDimensions.height - height;
  return {
    x: Math.max(pageDimensions.x, Math.min(location.x, maxX)),
    y: Math.max(pageDimensions.y, Math.min(location.y, maxY)),
  };
}

export class Ink2Manager extends EventTarget {
  private brush_: AnnotationBrush = {type: AnnotationBrushType.PEN};
  private stack_ = new UndoRedoStack(this);
  private listener_: EventListener;

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
      [TextStyle.STRIKETHROUGH]: false,
    },
  };
  private brushResolver_: PromiseResolver<void>|null = null;
  // Holds text attributes pre-populated from an existing annotation that the
  // user is editing. Null if the user is not editing an annotation or is
  // creating a new annotation using |attributes_|.
  private existingAnnotationAttributes_: TextAttributes|null = null;
  private clipboardAnnotation_: TextAnnotation|null = null;
  private isCutAnnotation_: boolean = false;
  private pluginController_: PluginController = PluginController.getInstance();
  private textResolver_: PromiseResolver<void>|null = null;
  private viewport_: Viewport|null = null;
  private nextAnnotationId_: number = 0;
  // Keeps track of fonts that have been sent to the backend so that each font
  // is only serialized and loaded once.
  private knownFontIds_: number[] = [];

  constructor() {
    super();
    this.listener_ = this.handleFinishInkStroke_.bind(this);
    this.pluginController_.getEventTarget().addEventListener(
        PluginControllerEventType.FINISH_INK_STROKE, this.listener_);
  }

  destroy() {
    this.pluginController_.getEventTarget().removeEventListener(
        PluginControllerEventType.FINISH_INK_STROKE, this.listener_);
  }

  setViewport(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  get annotations(): Map<number, Map<number, TextAnnotation>> {
    return this.annotations_;
  }

  resetStackForTesting() {
    this.stack_.resetForTesting();
  }

  // Initialize a new empty text annotation at `location` in screen coordinates.
  // No-op if there is no PDF page at `location`.
  // If `location` is not provided, creates the annotation at the center of the
  // visible portion of the most visible page.
  // Returns true if an annotation was initialized, and false otherwise.
  initializeTextAnnotation(location?: Point): boolean {
    assert(this.isTextInitializationComplete());
    assert(this.viewport_);

    const isMouse = !!location;
    const page = location ? this.viewport_.getPageAtPoint(location) :
                            this.viewport_.getMostVisiblePage();
    if (page === -1) {
      // Don't initialize an annotation if the click isn't on the PDF itself.
      return false;
    }

    const pageDimensions = this.viewport_.getPageScreenRect(page);
    // Enough space for 1 line of text. Default line height is around 1.2.
    const newBoxHeight = Math.max(
        MIN_TEXTBOX_SIZE_PX,
        Math.ceil(1.2 * this.attributes_.size * this.viewport_.getZoom()));
    let newBoxWidth = Math.min(
        DEFAULT_TEXTBOX_WIDTH,
        Math.max(MIN_TEXTBOX_SIZE_PX, pageDimensions.width));

    // Set location to the middle of the visible portion of the page.
    if (!location) {
      const minX = Math.max(pageDimensions.x, 0);
      const minY = Math.max(pageDimensions.y, 0);
      const maxX = Math.min(
          pageDimensions.x + pageDimensions.width, this.viewport_.size.width);
      const maxY = Math.min(
          pageDimensions.y + pageDimensions.height, this.viewport_.size.height);
      location = {
        x: Math.max(0, (minX + maxX) / 2 - newBoxWidth / 2),
        y: Math.max(0, (minY + maxY) / 2 - newBoxHeight / 2),
      };
    }

    // Adjust the location for a new annotation click so that the center of
    // the first line of text will align with the center of the cursor,
    // instead of the top left corner of the text aligning with the center of
    // the cursor. This does not apply for annotations created with the
    // keyboard.
    if (isMouse) {
      location.y =
          location.y - this.attributes_.size * this.viewport_.getZoom() / 2;
    }

    const minWidth = 2 * MIN_TEXTBOX_SIZE_PX;
    if (pageDimensions.width < minWidth ||
        pageDimensions.height < newBoxHeight) {
      // Don't try to create a new textbox if the visible page is too small.
      // The box needs to be big enough in screen coordinates to fit at
      // least some text, and Blink can't lay out arbitrarily small text
      // boxes.
      return false;
    }
    location =
        getClampedLocation(location, minWidth, newBoxHeight, pageDimensions);
    // Check if the box should be narrowed to fit in the page while being
    // as close as possible to the original click position.
    newBoxWidth = Math.min(
        newBoxWidth, pageDimensions.x + pageDimensions.width - location.x);

    const viewportRotations = this.viewport_.getClockwiseRotations();
    const annotation: TextAnnotation = {
      id: this.nextAnnotationId_,
      mojoTextInfo: new ArrayBuffer(0),
      pageIndex: page,
      pdfZoom: this.viewport_.getZoom(),
      text: '',
      textAttributes: structuredClone(this.attributes_),
      textBoxRect: {
        height: newBoxHeight,
        locationX: location.x,
        locationY: location.y,
        width: newBoxWidth,
      },
      textOrientation: (4 - viewportRotations) % 4,
      viewportOrientation: viewportRotations,
    };

    this.nextAnnotationId_++;
    this.initializeTextBox_(annotation, pageDimensions, /*isPaste=*/ false);
    return true;
  }

  private initializeTextBox_(
      annotation: TextAnnotation, pageDimensions: ViewportRect,
      isPaste: boolean) {
    this.existingAnnotationAttributes_ =
        isPaste ? structuredClone(annotation.textAttributes) : null;

    this.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation,
        pageDimensions,
        isPaste,
      },
    }));

    // Notify other listeners of any changes to the viewport and/or attributes,
    // since these may change with the annotation.
    this.fireAttributesChanged_();
  }

  saveAnnotationToClipboard(
      annotation: TextAnnotation, isCut: boolean = false) {
    assert(annotation.text !== '');
    this.clipboardAnnotation_ = structuredClone(annotation);
    this.isCutAnnotation_ = isCut;
    this.dispatchEvent(
        new CustomEvent('saved-annotation-to-clipboard-for-testing', {
          detail: {
            annotation: this.clipboardAnnotation_,
            isCut: this.isCutAnnotation_,
          },
        }));
  }

  // Pastes the clipboard annotation.
  // Returns true if an annotation was pasted, and false otherwise.
  pasteAnnotation(): boolean {
    assert(this.isTextInitializationComplete());
    assert(this.viewport_);

    if (!this.clipboardAnnotation_) {
      return false;
    }

    const page = this.clipboardAnnotation_.pageIndex;
    const pageDimensions = this.viewport_.getPageScreenRect(page);
    if (!pageDimensions) {
      return false;
    }

    const viewportRotations = this.viewport_.getClockwiseRotations();
    let id: number;
    if (this.isCutAnnotation_) {
      id = this.clipboardAnnotation_.id;
      this.nextAnnotationId_ = Math.max(this.nextAnnotationId_, id + 1);
      this.isCutAnnotation_ = false;
    } else {
      id = this.nextAnnotationId_++;
    }

    const screenRect = pageToScreenCoordinates(
        page, this.clipboardAnnotation_.textBoxRect, this.viewport_);

    const location = getClampedLocation(
        {x: screenRect.locationX + 10, y: screenRect.locationY + 10},
        screenRect.width, screenRect.height, pageDimensions);

    const textBoxRect: TextBoxRect = {
      height: screenRect.height,
      locationX: location.x,
      locationY: location.y,
      width: screenRect.width,
    };

    const annotation: TextAnnotation = {
      id,
      mojoTextInfo: this.clipboardAnnotation_.mojoTextInfo,
      pageIndex: page,
      pdfZoom: this.viewport_.getZoom(),
      text: this.clipboardAnnotation_.text,
      textAttributes: structuredClone(this.clipboardAnnotation_.textAttributes),
      textBoxRect,
      textOrientation: this.clipboardAnnotation_.textOrientation,
      viewportOrientation: viewportRotations,
    };

    // Update clipboard coordinates for subsequent consecutive pastes (+10px
    // cascading).
    const pageRect = screenToPageCoordinates(page, textBoxRect, this.viewport_);
    this.clipboardAnnotation_ = structuredClone(annotation);
    this.clipboardAnnotation_.textBoxRect = pageRect;

    this.initializeTextBox_(annotation, pageDimensions, /*isPaste=*/ true);
    return true;
  }

  // Reactivate an existing text annotation for editing.
  reactivateTextAnnotation(annotation: TextAnnotation) {
    assert(this.isTextInitializationComplete());
    this.pluginController_.editTextAnnotation(annotation.id);
    this.existingAnnotationAttributes_ =
        structuredClone(annotation.textAttributes);
    this.fireAttributesChanged_();
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
        let pageMap = this.annotations_.get(annotation.pageIndex);
        if (!pageMap) {
          pageMap = new Map();
          this.annotations_.set(annotation.pageIndex, pageMap);
        }
        pageMap.set(annotation.id, annotation);
        this.nextAnnotationId_ =
            Math.max(this.nextAnnotationId_, annotation.id + 1);
      });
      this.textResolver_!.resolve();
      this.dispatchEvent(new CustomEvent('annotations-updated'));
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

  toggleTextStyle(style: TextStyle) {
    const current = this.getCurrentTextAttributes();
    const newStyles = Object.assign({}, current.styles);
    newStyles[style] = !newStyles[style];
    this.setTextStyles(newStyles);
  }

  getKnownFontIds(): number[] {
    return [...this.knownFontIds_];
  }

  addKnownFontId(id: number) {
    assert(!this.knownFontIds_.includes(id));
    this.knownFontIds_.push(id);
  }

  // Returns the previous version of the annotation, or null if it is new.
  private updateStoredAnnotation_(annotation: TextAnnotation): TextAnnotation
      |null {
    let pageAnnotations = this.annotations_.get(annotation.pageIndex);
    if (!pageAnnotations) {
      // Adding a new annotation, on a page that doesn't have any existing ones.
      // Create and add the new map.
      pageAnnotations = new Map();
      this.annotations_.set(annotation.pageIndex, pageAnnotations);
    }

    const previous = pageAnnotations.get(annotation.id);
    if (annotation.text === '') {
      // Delete an existing annotation.
      assert(pageAnnotations.delete(annotation.id));
    } else {
      pageAnnotations.set(annotation.id, annotation);
    }
    return previous || null;
  }

  /**
   * Updates the stored annotation and notifies the plugin of the new or
   * modified annotation.
   */
  commitTextAnnotation(
      annotation: TextAnnotation, isEdited: boolean,
      newTypefaces: chrome.pdfViewerPrivate.Typeface[]) {
    assert(this.viewport_);
    annotation.textBoxRect = screenToPageCoordinates(
        annotation.pageIndex, annotation.textBoxRect, this.viewport_);

    if (isEdited) {
      const before = this.updateStoredAnnotation_(annotation);
      const after = annotation.text === '' ? null : structuredClone(annotation);
      if (before !== null || after !== null) {
        this.stack_.push({
          type: 'text',
          before,
          after,
        });
      }
    }

    const messageData: TextAnnotationMessageData = {
      ...annotation,
      isEdited,
      newTypefaces,
      source: TextAnnotationSource.USER,
    };
    this.pluginController_.finishTextAnnotation(messageData);
    this.existingAnnotationAttributes_ = null;
    this.dispatchEvent(new CustomEvent('annotations-updated'));
    this.fireAttributesChanged_();
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

  private handleFinishInkStroke_(e: Event) {
    if ((e as CustomEvent<boolean>).detail) {
      this.stack_.push({type: 'ink'});
    }
  }

  // Note: Undo/Redo state is tracked for all annotations, but changes are only
  // applied by the frontend for text annotations. Undo/redo of ink strokes are
  // handled exclusively by the backend.
  undo() {
    const state = this.stack_.undo();
    if (!state) {
      return;
    }

    if (state.type === 'text') {
      this.applyTextUndoRedo_(
          state.before, state.after, TextAnnotationSource.UNDO);
    }
    this.pluginController_.undo();
  }

  redo() {
    const state = this.stack_.redo();
    if (!state) {
      return;
    }

    if (state.type === 'text') {
      this.applyTextUndoRedo_(
          state.after, state.before, TextAnnotationSource.REDO);
    }
    this.pluginController_.redo();
  }

  private applyTextUndoRedo_(
      update: TextAnnotation|null, previous: TextAnnotation|null,
      source: TextAnnotationSource) {
    const isDeletion = update === null;
    // If deleting, the relevant annotation is the "previous" one, which is
    // being deleted. Otherwise, the relevant annotation is the update.
    const annotation = isDeletion ? previous : update;
    assert(annotation);
    assert(this.viewport_);

    // Since this is an undo or redo of a previous change, the map for this page
    // should always have been created already.
    const pageAnnotations = this.annotations_.get(annotation.pageIndex);
    assert(pageAnnotations);
    if (isDeletion) {
      pageAnnotations.delete(annotation.id);
    } else {
      pageAnnotations.set(annotation.id, annotation);
    }

    const messageData: TextAnnotationMessageData = {
      ...annotation,
      isEdited: true,
      newTypefaces: [],
      source,
    };
    if (isDeletion) {
      messageData.text = '';
    }
    this.pluginController_.finishTextAnnotation(messageData);
    this.dispatchEvent(new CustomEvent('annotations-updated'));
  }

  initiateSave() {
    this.stack_.initiateSave();
  }

  cancelSave() {
    this.stack_.cancelSave();
  }

  saved() {
    this.stack_.setSaved();
  }

  static getInstance(): Ink2Manager {
    return instance || (instance = new Ink2Manager());
  }

  static setInstance(obj: Ink2Manager|null) {
    if (instance) {
      instance.destroy();
    }
    instance = obj;
  }
}

let instance: (Ink2Manager|null) = null;

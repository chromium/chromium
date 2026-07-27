// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation, TextAttributes, TextBoxRect} from '../constants.js';
import {TextTypeface} from '../constants.js';
import {Ink2Manager, MIN_TEXTBOX_SIZE_PX, stylesEqual} from '../ink2_manager.js';
import {convertRotatedCoordinates} from '../ink_text_annotation_utils.js';
import {record, UserAction} from '../metrics.js';
import {PdfViewerPrivateProxyImpl} from '../pdf_viewer_private_proxy.js';
import {colorsEqual, colorToHex, hasCtrlModifier} from '../pdf_viewer_utils.js';
import type {Viewport, ViewportRect} from '../viewport.js';

import {getCss} from './ink_text_box.css.js';
import {getHtml} from './ink_text_box.html.js';
import {InkTextObserverMixin} from './ink_text_observer_mixin.js';

export interface InkTextBoxElement {
  $: {
    textbox: HTMLTextAreaElement,
  };
}

export enum TextBoxState {
  INACTIVE = 0,  // No active text annotation being edited; box is hidden.
  NEW = 1,  // Box initialized with an annotation, but user has not made edits.
  EDITED = 2,  // User has edited the annotation (position, text, style).
}

const KEYBOARD_RESIZE_STEP_PX = 10;

function getStyleForTypeface(typeface: TextTypeface): string {
  switch (typeface) {
    case TextTypeface.SANS_SERIF:
      return 'Arial, sans-serif';
    case TextTypeface.SERIF:
      return 'Times, serif';
    case TextTypeface.MONOSPACE:
      return '"Courier New", monospace';
    default:
      assertNotReachedCase(typeface);
  }
}

const InkTextBoxElementBase = InkTextObserverMixin(CrLitElement);

export class InkTextBoxElement extends InkTextBoxElementBase {
  static get is() {
    return 'ink-text-box';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      height_: {type: Number},
      locationX_: {type: Number},
      locationY_: {type: Number},
      minHeight_: {type: Number},
      minWidth_: {type: Number},
      state_: {type: Number},
      textOrientation_: {type: Number},
      textRotations_: {
        type: Number,
        reflect: true,
      },
      textValue_: {type: String},
      viewportRotations_: {type: Number},
      width_: {type: Number},
      zoom_: {type: Number},
      viewport: {type: Object},
      annotation: {type: Object},
      pageDimensions: {type: Object},
    };
  }

  // Note: locationX_, locationY_, minHeight_, minWidth_, height_ and width_
  // are in screen coordinates.
  private accessor locationX_: number = 0;
  private accessor locationY_: number = 0;
  private accessor minHeight_: number = MIN_TEXTBOX_SIZE_PX;
  private accessor minWidth_: number = MIN_TEXTBOX_SIZE_PX;
  private accessor height_: number = MIN_TEXTBOX_SIZE_PX;
  private accessor state_: TextBoxState = TextBoxState.INACTIVE;
  private accessor textOrientation_: number = 0;
  protected accessor textRotations_: number = 0;
  protected accessor textValue_: string = '';
  private accessor viewportRotations_: number = 0;
  private accessor width_: number = MIN_TEXTBOX_SIZE_PX;
  private accessor zoom_: number = 1.0;
  accessor viewport: Viewport|null = null;
  accessor annotation: TextAnnotation|null = null;
  accessor pageDimensions: ViewportRect|null = null;

  private attributes_?: TextAttributes;
  private currentArrowKey_: string|null = null;
  private dragTarget_: HTMLElement|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  // Whether this is an existing textbox. Tracked so that the textbox can
  // correctly notify the backend about changes (e.g. deleting all text in an
  // existing annotation should remove it from the PDF, so this change must be
  // committed where an empty new annotation would not be committed).
  private existing_: boolean = false;
  private id_: number = -1;
  private keyDownCount_: number = -1;
  private pageIndex_: number = -1;
  private pageHeight_: number = 0;
  private pageWidth_: number = 0;
  private pageX_: number = 0;
  private pageY_: number = 0;
  private pointerStart_: {x: number, y: number}|null = null;
  private startPosition_: TextBoxRect|null = null;
  private promiseResolver_: PromiseResolver<void>|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        this, 'pointerdown', (e: PointerEvent) => this.onPointerDown_(e));
    this.eventTracker_.add(
        this, 'keydown', (e: KeyboardEvent) => this.onKeyDown_(e));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('annotation') ||
        changedProperties.has('pageDimensions')) {
      if (this.annotation && this.pageDimensions) {
        this.initializeFromProperties_();
      } else {
        this.state_ = TextBoxState.INACTIVE;
      }
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('minHeight_')) {
      this.height_ = this.getClampedHeight_(this.height_);
    }

    if (changedPrivateProperties.has('minWidth_')) {
      this.width_ = this.getClampedWidth_(this.width_);
    }

    if (changedPrivateProperties.has('state_')) {
      this.hidden = this.state_ === TextBoxState.INACTIVE;
      this.fire('state-changed', this.state_);
    }

    if (changedPrivateProperties.has('viewportRotations_') ||
        changedPrivateProperties.has('textOrientation_')) {
      this.textRotations_ =
          (this.viewportRotations_ + this.textOrientation_) % 4;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.setAttribute('tabindex', '0');
    this.addEventListener('focus', e => this.onFocus_(e));
    document.addEventListener('keydown', e => this.onDocumentKeyDown_(e));
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('width_')) {
      this.$.textbox.style.width = `${this.width_}px`;
    }
    if (changedPrivateProperties.has('height_')) {
      this.$.textbox.style.height = `${this.height_}px`;
    }
    if (changedPrivateProperties.has('locationX_')) {
      this.style.setProperty('--textbox-location-x', `${this.locationX_}px`);
    }
    if (changedPrivateProperties.has('locationY_')) {
      this.style.setProperty('--textbox-location-y', `${this.locationY_}px`);
    }
    if (changedPrivateProperties.has('zoom_')) {
      this.styleFontSize_();
    }
    if ((changedPrivateProperties.has('width_') &&
         this.textRotations_ % 2 === 0) ||
        (changedPrivateProperties.has('height_') &&
         this.textRotations_ % 2 === 1)) {
      this.updateMinimumSize_();
    }
  }

  private styleFontSize_() {
    if (this.attributes_) {
      this.$.textbox.style.fontSize = `${this.attributes_.size * this.zoom_}px`;
    }
  }

  protected onFocus_(e: FocusEvent) {
    if (e.relatedTarget === this ||
        e.currentTarget === this && e.relatedTarget === this.$.textbox) {
      // Focus moved from the box to the textarea (or vice versa via the
      // "Escape" shortcut), ignore.
      return;
    }

    this.fire('textbox-focused', {
      height: this.height_,
      locationX: this.locationX_,
      locationY: this.locationY_,
      width: this.width_,
    });
  }

  protected onTextValueInput_() {
    this.textValue_ = this.$.textbox.value;
    this.textBoxEdited_();
    this.updateMinimumSize_();
  }

  private textBoxEdited_() {
    if (this.state_ === TextBoxState.NEW) {
      this.state_ = TextBoxState.EDITED;
    }
  }

  private updateMinimumSize_() {
    if (this.textRotations_ % 2 === 0) {
      this.$.textbox.style.height = 'auto';
      const scrollHeight = this.$.textbox.scrollHeight;
      this.minHeight_ = Math.max(MIN_TEXTBOX_SIZE_PX, scrollHeight);
      // Reset the height styling back.
      this.$.textbox.style.height = `${this.height_}px`;
    } else {
      // Adjust the width if the user is typing vertically.
      this.$.textbox.style.width = 'auto';
      const scrollWidth = this.$.textbox.scrollWidth;
      this.minWidth_ = Math.max(MIN_TEXTBOX_SIZE_PX, scrollWidth);
      // Reset the width styling back.
      this.$.textbox.style.width = `${this.width_}px`;
    }
  }

  private removePointerDragListeners_() {
    assert(this.dragTarget_);
    this.eventTracker_.remove(this.dragTarget_, 'pointercancel');
    this.eventTracker_.remove(this.dragTarget_, 'pointerup');
    this.eventTracker_.remove(this.dragTarget_, 'pointermove');
    this.dragTarget_ = null;
    this.pointerStart_ = null;
  }

  private removeKeyDragListeners_() {
    assert(this.dragTarget_);
    this.eventTracker_.remove(this.dragTarget_, 'keyup');
    this.eventTracker_.remove(this.dragTarget_, 'focusout');
    this.dragTarget_ = null;
    this.currentArrowKey_ = null;
    this.keyDownCount_ = -1;
  }

  // Removes any drag listeners and resets location to the start position.
  private resetDrag_() {
    if (this.dragTarget_ === null) {
      return;
    }

    // Reset location to the start position.
    assert(this.startPosition_);
    this.locationX_ = this.startPosition_.locationX;
    this.locationY_ = this.startPosition_.locationY;
    this.width_ = this.startPosition_.width;
    this.height_ = this.startPosition_.height;
    this.startPosition_ = null;

    if (this.pointerStart_ !== null) {
      this.removePointerDragListeners_();
    } else if (this.currentArrowKey_ !== null) {
      this.removeKeyDragListeners_();
    }
  }

  commitTextAnnotation(): Promise<void> {
    if (this.promiseResolver_) {
      return this.promiseResolver_.promise;
    }

    this.promiseResolver_ = new PromiseResolver<void>();
    const promise = this.promiseResolver_.promise;

    this.resetDrag_();

    if ((this.state_ !== TextBoxState.EDITED || this.textValue_ === '') &&
        !this.existing_) {
      // Empty textbox.
      this.finishCommit_();
      return promise;
    }

    // Save the existing state with dummy mojoTextInfo.
    assert(this.attributes_);
    const isEdited = this.state_ === TextBoxState.EDITED;
    const annotation: TextAnnotation = {
      id: this.id_,
      mojoTextInfo: new ArrayBuffer(0),
      pageIndex: this.pageIndex_,
      pdfZoom: this.zoom_,
      text: this.textValue_,
      textAttributes: structuredClone(this.attributes_),
      textBoxRect: {
        height: this.height_,
        locationX: this.locationX_,
        locationY: this.locationY_,
        width: this.width_,
      },
      textOrientation: this.textOrientation_,
      viewportOrientation: this.viewportRotations_,
    };

    if (!isEdited) {
      // No edits.
      Ink2Manager.getInstance().commitTextAnnotation(
          annotation, isEdited, /*typefaces=*/[]);
      this.finishCommit_();
      return promise;
    }

    // Has edits.
    (async () => {
      try {
        const result =
            await PdfViewerPrivateProxyImpl.getInstance().getTextInfo(
                this.$.textbox, Ink2Manager.getInstance().getKnownFontIds());

        for (const typeface of result.typefaces) {
          Ink2Manager.getInstance().addKnownFontId(typeface.uniqueId);
        }

        annotation.mojoTextInfo = result.mojoTextInfo;
        Ink2Manager.getInstance().commitTextAnnotation(
            annotation, isEdited, result.typefaces);
        if (!this.existing_) {
          record(UserAction.ADD_INK2_TEXT_ANNOTATION);
        }
      } catch (e) {
        console.error('Error committing text annotation:', e);
      } finally {
        this.finishCommit_();
      }
    })();

    return promise;
  }

  private finishCommit_() {
    this.state_ = TextBoxState.INACTIVE;
    assert(this.promiseResolver_);
    this.promiseResolver_.resolve();
    this.promiseResolver_ = null;
  }

  private initializeFromProperties_() {
    const annotation = this.annotation;
    const pageDimensions = this.pageDimensions;
    if (!annotation || !pageDimensions) {
      return;
    }

    if (this.viewport) {
      this.zoom_ = this.viewport.getZoom();
      this.viewportRotations_ = this.viewport.getClockwiseRotations();
    }

    // Update is in screen coordinates.
    this.pageX_ = pageDimensions.x;
    this.pageY_ = pageDimensions.y;
    this.pageWidth_ = pageDimensions.width;
    this.pageHeight_ = pageDimensions.height;
    this.width_ = annotation.textBoxRect.width;
    this.height_ = annotation.textBoxRect.height;
    this.minHeight_ = MIN_TEXTBOX_SIZE_PX;
    this.minWidth_ = MIN_TEXTBOX_SIZE_PX;
    this.locationX_ = annotation.textBoxRect.locationX;
    this.locationY_ = annotation.textBoxRect.locationY;
    this.state_ = TextBoxState.NEW;
    this.existing_ = annotation.text !== '';
    this.textValue_ = annotation.text;
    this.id_ = annotation.id;
    this.pageIndex_ = annotation.pageIndex;
    this.textOrientation_ = annotation.textOrientation;
    this.updateTextAttributes_(annotation.textAttributes);

    this.focusTextboxWhenReady_();
  }

  private async focusTextboxWhenReady_() {
    await this.updateComplete;
    setTimeout(() => {
      this.$.textbox.focus();
      this.fire('textbox-focused-for-test');
    }, 0);
  }

  viewportChanged() {
    if (!this.viewport || this.pageIndex_ === -1) {
      return;
    }
    const zoom = this.viewport.getZoom();
    const clockwiseRotations = this.viewport.getClockwiseRotations();
    const pageDimensions = this.viewport.getPageScreenRect(this.pageIndex_);

    // Convert width, height, locationX, locationY to the new screen
    // coordinates.

    // Note that this.pageX_ and this.pageY_ are in the old screen
    // coordinates, i.e. they were using the old zoom value.
    const adjusted = {
      locationX: (this.locationX_ - this.pageX_) * zoom / this.zoom_,
      locationY: (this.locationY_ - this.pageY_) * zoom / this.zoom_,
      width: Math.max(this.width_ * zoom / this.zoom_, MIN_TEXTBOX_SIZE_PX),
      height: Math.max(this.height_ * zoom / this.zoom_, MIN_TEXTBOX_SIZE_PX),
    };
    const rotated = convertRotatedCoordinates(
        adjusted, this.viewportRotations_, clockwiseRotations,
        pageDimensions.width, pageDimensions.height);
    // Flip min height and width if orientation has switched.
    if (this.viewportRotations_ % 2 !== clockwiseRotations % 2) {
      const min = this.minHeight_;
      this.minHeight_ = this.minWidth_;
      this.minWidth_ = min;
    }
    this.locationX_ = rotated.locationX + pageDimensions.x;
    this.locationY_ = rotated.locationY + pageDimensions.y;
    this.width_ = rotated.width;
    this.height_ = rotated.height;

    // Update properties to the new values.
    this.viewportRotations_ = clockwiseRotations;
    this.zoom_ = zoom;
    this.pageX_ = pageDimensions.x;
    this.pageY_ = pageDimensions.y;
    this.pageWidth_ = pageDimensions.width;
    this.pageHeight_ = pageDimensions.height;
  }

  private onDocumentKeyDown_(e: KeyboardEvent) {
    // Only handle "Escape" when in an active state.
    if (e.key !== 'Escape' || this.state_ === TextBoxState.INACTIVE) {
      return;
    }

    const target = e.composedPath()[0];
    if (target === this.$.textbox) {
      this.focus();
      this.fire('ink-text-box-focused-for-test');
    } else {
      this.commitTextAnnotation();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  private onKeyDown_(e: KeyboardEvent) {
    const target = e.composedPath()[0];
    // Ignore keyboard events on the textbox itself, other than 'Escape', which
    // is separately handled by the global keyhandler above.
    if (!(target instanceof HTMLElement) || target === this.$.textbox) {
      return;
    }

    // Backspace/Delete key not in the textbox deletes the annotation.
    if (e.key === 'Backspace' || e.key === 'Delete') {
      this.textValue_ = '';
      this.textBoxEdited_();
      this.commitTextAnnotation();
      return;
    }

    // Ignore if the user is already dragging with the pointer.
    if (this.pointerStart_ !== null) {
      return;
    }

    if (this.handleResizeShortcut_(e)) {
      e.preventDefault();
      e.stopPropagation();
      return;
    }

    // Ignore all other keys except arrows.
    if (!['ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight'].includes(e.key)) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    // Reset state if this is a new arrow key.
    if (this.currentArrowKey_ !== null && this.currentArrowKey_ !== e.key) {
      this.onHandleKeyUp_();
    }
    this.currentArrowKey_ = e.key;

    if (this.keyDownCount_ === -1) {
      this.dragTarget_ = this;
      this.eventTracker_.add(this, 'keyup', () => this.onHandleKeyUp_());
      this.eventTracker_.add(this, 'focusout', () => this.onHandleKeyUp_());
      this.keyDownCount_ = 0;
      this.startPosition_ = {
        locationX: this.locationX_,
        locationY: this.locationY_,
        width: this.width_,
        height: this.height_,
      };
    }
    this.keyDownCount_++;

    let moveX = 0;
    let moveY = 0;
    switch (e.key) {
      case 'ArrowDown':
        moveY = this.keyDownCount_;
        break;
      case 'ArrowUp':
        moveY = -1 * this.keyDownCount_;
        break;
      case 'ArrowLeft':
        moveX = -1 * this.keyDownCount_;
        break;
      case 'ArrowRight':
        moveX = this.keyDownCount_;
        break;
      default:
        break;
    }
    this.onMove_(this, moveX, moveY);
  }

  private onHandleKeyUp_() {
    this.startPosition_ = null;
    this.removeKeyDragListeners_();
    this.textBoxEdited_();
  }

  protected onPointerDown_(e: PointerEvent) {
    const target = e.composedPath()[0];
    // Ignore pointer events on the textbox itself.
    if (e.button !== 0 || !(target instanceof HTMLElement) ||
        target === this.$.textbox) {
      return;
    }

    // Don't allow dragging with the keyboard and pointer at the same time.
    if (this.dragTarget_ !== null) {
      return;
    }

    this.dragTarget_ = target;
    this.pointerStart_ = {x: e.x, y: e.y};
    this.startPosition_ = {
      locationX: this.locationX_,
      locationY: this.locationY_,
      width: this.width_,
      height: this.height_,
    };

    this.eventTracker_.add(
        target, 'pointercancel', () => this.onHandlePointerUp_());
    this.eventTracker_.add(
        target, 'pointerup', () => this.onHandlePointerUp_());
    this.eventTracker_.add(
        target, 'pointermove',
        (e: PointerEvent) => this.onHandlePointerMove_(e));
    target.setPointerCapture(e.pointerId);
  }

  private onHandlePointerMove_(e: PointerEvent) {
    const target = e.target as HTMLElement;
    assert(this.pointerStart_);
    this.onMove_(
        target, e.x - this.pointerStart_.x, e.y - this.pointerStart_.y);
  }

  private onMove_(target: HTMLElement, moveX: number, moveY: number) {
    assert(this.startPosition_);
    if (!target.classList.contains('handle')) {
      // User is dragging the box itself.
      this.locationX_ = Math.min(
          this.pageX_ + this.pageWidth_ - this.width_,
          Math.max(this.pageX_, this.startPosition_.locationX + moveX));
      this.locationY_ = Math.min(
          this.pageY_ + this.pageHeight_ - this.height_,
          Math.max(this.pageY_, this.startPosition_.locationY + moveY));
      return;
    }

    if (target.classList.contains('left')) {
      const deltaX = Math.max(
          this.pageX_ - this.startPosition_.locationX,
          Math.min(moveX, this.startPosition_.width - this.minWidth_));
      this.locationX_ = this.startPosition_.locationX + deltaX;
      this.width_ = this.startPosition_.width - deltaX;
    } else if (target.classList.contains('right')) {
      this.width_ = this.getClampedWidth_(this.startPosition_.width + moveX);
    }
    if (target.classList.contains('top')) {
      const deltaY = Math.max(
          this.pageY_ - this.startPosition_.locationY,
          Math.min(moveY, this.startPosition_.height - this.minHeight_));
      this.locationY_ = this.startPosition_.locationY + deltaY;
      this.height_ = this.startPosition_.height - deltaY;
    } else if (target.classList.contains('bottom')) {
      this.height_ = this.getClampedHeight_(this.startPosition_.height + moveY);
    }
  }

  private onHandlePointerUp_() {
    this.startPosition_ = null;
    this.removePointerDragListeners_();
    this.textBoxEdited_();
  }

  private updateTextAttributes_(newAttributes: TextAttributes) {
    this.$.textbox.style.fontFamily =
        getStyleForTypeface(newAttributes.typeface);
    this.attributes_ = newAttributes;
    this.styleFontSize_();
    this.$.textbox.style.textAlign = newAttributes.alignment;
    this.$.textbox.style.fontStyle =
        newAttributes.styles.italic ? 'italic' : 'normal';
    this.$.textbox.style.fontWeight =
        newAttributes.styles.bold ? 'bold' : 'normal';
    this.$.textbox.style.color = colorToHex(newAttributes.color);
  }

  override onTextAttributesChanged(newAttributes: TextAttributes) {
    if (!!this.attributes_ &&
        newAttributes.typeface === this.attributes_.typeface &&
        newAttributes.size === this.attributes_.size &&
        colorsEqual(newAttributes.color, this.attributes_.color) &&
        newAttributes.alignment === this.attributes_.alignment &&
        stylesEqual(newAttributes.styles, this.attributes_.styles)) {
      return;
    }

    this.updateTextAttributes_(newAttributes);
    this.textBoxEdited_();
    if (this.state_ !== TextBoxState.INACTIVE) {
      this.updateMinimumSize_();
    }
  }

  private handleResizeShortcut_(e: KeyboardEvent): boolean {
    if (this.state_ === TextBoxState.INACTIVE) {
      return false;
    }

    const mainModifier = hasCtrlModifier(e);
    const secondModifier = isMac ? e.ctrlKey : e.altKey;

    if (!mainModifier || !secondModifier || e.shiftKey) {
      return false;
    }

    switch (e.key.toLowerCase()) {
      case 'b':
        this.resizeBy_(KEYBOARD_RESIZE_STEP_PX, 0);
        return true;
      case 'w':
        this.resizeBy_(-KEYBOARD_RESIZE_STEP_PX, 0);
        return true;
      case 'i':
        this.resizeBy_(0, KEYBOARD_RESIZE_STEP_PX);
        return true;
      case '9':
        this.resizeBy_(0, -KEYBOARD_RESIZE_STEP_PX);
        return true;
      case 'k':
        this.resizeProportionally_(1.1);
        return true;
      case 'j':
        this.resizeProportionally_(0.9);
        return true;
      default:
        return false;
    }
  }

  /**
   * Changes the size of the textbox by deltaX px horizontally and deltaY px
   * vertically.
   */
  private resizeBy_(deltaX: number, deltaY: number) {
    const newWidth = this.getClampedWidth_(this.width_ + deltaX);
    const newHeight = this.getClampedHeight_(this.height_ + deltaY);

    if (newWidth !== this.width_ || newHeight !== this.height_) {
      this.width_ = newWidth;
      this.height_ = newHeight;
      this.textBoxEdited_();
    }
  }

  private resizeProportionally_(scale: number) {
    const clampedScale = scale > 1 ?
        Math.min(
            scale, this.getMaxWidth_() / this.width_,
            this.getMaxHeight_() / this.height_) :
        Math.max(
            scale, this.minWidth_ / this.width_,
            this.minHeight_ / this.height_);

    const newWidth = Math.round(this.width_ * clampedScale);
    const newHeight = Math.round(this.height_ * clampedScale);

    if (newWidth !== this.width_ || newHeight !== this.height_) {
      this.width_ = newWidth;
      this.height_ = newHeight;
      this.textBoxEdited_();
    }
  }

  private getMaxWidth_(): number {
    return this.pageWidth_ + this.pageX_ - this.locationX_;
  }

  private getMaxHeight_(): number {
    return this.pageHeight_ + this.pageY_ - this.locationY_;
  }

  private getClampedWidth_(width: number): number {
    return Math.min(this.getMaxWidth_(), Math.max(this.minWidth_, width));
  }

  private getClampedHeight_(height: number): number {
    return Math.min(this.getMaxHeight_(), Math.max(this.minHeight_, height));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-box': InkTextBoxElement;
  }
}

customElements.define(InkTextBoxElement.is, InkTextBoxElement);

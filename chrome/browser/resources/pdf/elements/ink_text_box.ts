// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes, TextBoxRect} from '../constants.js';
import {TextTypeface} from '../constants.js';
import {colorsEqual, convertRotatedCoordinates, Ink2Manager, MIN_TEXTBOX_SIZE_PX, stylesEqual} from '../ink2_manager.js';
import type {TextBoxInit, ViewportParams} from '../ink2_manager.js';
import {colorToHex} from '../pdf_viewer_utils.js';

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

function getStyleForTypeface(typeface: TextTypeface): string {
  switch (typeface) {
    case TextTypeface.SANS_SERIF:
      return 'Arial, sans-serif';
    case TextTypeface.SERIF:
      return 'Times, serif';
    case TextTypeface.MONOSPACE:
      return '"Courier New", monospace';
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

  private attributes_?: TextAttributes;
  private currentArrowKey_: string|null = null;
  private dragTarget_: HTMLElement|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  // Whether this is an existing textbox. Tracked so that the textbox can
  // correctly notify the backend about changes (e.g. deleting all text in an
  // existing annotation should remove it from the PDF, so we need to commit
  // this change where we wouldn't commit an empty new annotation).
  private existing_: boolean = false;
  private id_: number = -1;
  private keyDownCount_: number = -1;
  private pageNumber_: number = -1;
  private pageHeight_: number = 0;
  private pageWidth_: number = 0;
  private pageX_: number = 0;
  private pageY_: number = 0;
  private pointerStart_: {x: number, y: number}|null = null;
  private startPosition_: TextBoxRect|null = null;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.setAttribute('tabindex', '0');
    this.addEventListener('focus', e => this.onFocus_(e));
    document.addEventListener('keydown', e => this.onDocumentKeyDown_(e));
  }

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'initialize-text-box',
        (e: Event) =>
            this.onInitializeTextBox_((e as CustomEvent<TextBoxInit>).detail));
    this.onViewportChanged_(Ink2Manager.getInstance().getViewportParams());
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'viewport-changed',
        (e: Event) =>
            this.onViewportChanged_((e as CustomEvent<ViewportParams>).detail));
    this.eventTracker_.add(
        this, 'pointerdown', (e: PointerEvent) => this.onPointerDown_(e));
    this.eventTracker_.add(
        this, 'keydown', (e: KeyboardEvent) => this.onKeyDown_(e));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // This element is disconnected when the user exits text annotation mode.
    // Send the current annotation to the backend.
    this.commitTextAnnotation();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('minHeight_')) {
      this.height_ = Math.min(
          this.pageHeight_ + this.pageY_ - this.locationY_,
          Math.max(this.height_, this.minHeight_));
    }

    if (changedPrivateProperties.has('minWidth_')) {
      this.width_ = Math.min(
          this.pageWidth_ + this.pageX_ - this.locationX_,
          Math.max(this.width_, this.minWidth_));
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

    Ink2Manager.getInstance().textBoxFocused({
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

  commitTextAnnotation() {
    // If the user is still dragging the box by holding down a key or pointer,
    // reset location to the start of the drag and remove listeners before
    // deactivating the box and committing the annotation.
    this.resetDrag_();

    // If this is a new/inactive box or a new box edited to empty, nothing to do
    // unless it was initialized from an existing annotation. If this was
    // an existing annotation, we need to notify the backend to re-render it,
    // if unchanged, or delete it, if the text was set to empty.
    if ((this.state_ !== TextBoxState.EDITED || this.textValue_ === '') &&
        !this.existing_) {
      this.state_ = TextBoxState.INACTIVE;
      return;
    }

    // Notify the backend.
    assert(this.attributes_);
    Ink2Manager.getInstance().commitTextAnnotation(
        {
          text: this.textValue_,
          id: this.id_,
          pageNumber: this.pageNumber_,
          textAttributes: this.attributes_,
          textBoxRect: {
            height: this.height_,
            locationX: this.locationX_,
            locationY: this.locationY_,
            width: this.width_,
          },
          textOrientation: this.textOrientation_,
        },
        this.state_ === TextBoxState.EDITED);

    this.state_ = TextBoxState.INACTIVE;
  }

  private onInitializeTextBox_(data: TextBoxInit) {
    // If we are already editing an annotation, commit it first before
    // switching to the new one.
    if (this.state_ !== TextBoxState.INACTIVE) {
      this.commitTextAnnotation();
    }

    // Update is in screen coordinates.
    this.pageX_ = data.pageDimensions.x;
    this.pageY_ = data.pageDimensions.y;
    this.pageWidth_ = data.pageDimensions.width;
    this.pageHeight_ = data.pageDimensions.height;
    this.width_ = data.annotation.textBoxRect.width;
    this.height_ = data.annotation.textBoxRect.height;
    this.minHeight_ = MIN_TEXTBOX_SIZE_PX;
    this.minWidth_ = MIN_TEXTBOX_SIZE_PX;
    this.locationX_ = data.annotation.textBoxRect.locationX;
    this.locationY_ = data.annotation.textBoxRect.locationY;
    this.state_ = TextBoxState.NEW;
    this.existing_ = data.annotation.text !== '';
    this.textValue_ = data.annotation.text;
    this.id_ = data.annotation.id;
    this.pageNumber_ = data.annotation.pageNumber;
    this.textOrientation_ = data.annotation.textOrientation;
    this.updateTextAttributes_(data.annotation.textAttributes);

    this.focusTextboxWhenReady_();
  }

  private async focusTextboxWhenReady_() {
    await this.updateComplete;
    setTimeout(() => {
      this.$.textbox.focus();
      this.fire('textbox-focused-for-test');
    }, 0);
  }

  private onViewportChanged_(update: ViewportParams) {
    // Convert width, height, locationX, locationY to the new screen
    // coordinates.

    // Note that this.pageX_ and this.pageY_ are in the old screen
    // coordinates, i.e. they were using the old zoom value.
    const adjusted = {
      locationX: (this.locationX_ - this.pageX_) * update.zoom / this.zoom_,
      locationY: (this.locationY_ - this.pageY_) * update.zoom / this.zoom_,
      width:
          Math.max(this.width_ * update.zoom / this.zoom_, MIN_TEXTBOX_SIZE_PX),
      height: Math.max(
          this.height_ * update.zoom / this.zoom_, MIN_TEXTBOX_SIZE_PX),
    };
    const rotated = convertRotatedCoordinates(
        adjusted, this.viewportRotations_, update.clockwiseRotations,
        update.pageDimensions.width, update.pageDimensions.height);
    // Flip min height and width if we've switched orientation.
    if (this.viewportRotations_ % 2 !== update.clockwiseRotations % 2) {
      const min = this.minHeight_;
      this.minHeight_ = this.minWidth_;
      this.minWidth_ = min;
    }
    this.locationX_ = rotated.locationX + update.pageDimensions.x;
    this.locationY_ = rotated.locationY + update.pageDimensions.y;
    this.width_ = rotated.width;
    this.height_ = rotated.height;

    // Update properties to the new values.
    this.viewportRotations_ = update.clockwiseRotations;
    this.zoom_ = update.zoom;
    this.pageX_ = update.pageDimensions.x;
    this.pageY_ = update.pageDimensions.y;
    this.pageWidth_ = update.pageDimensions.width;
    this.pageHeight_ = update.pageDimensions.height;
  }

  private onDocumentKeyDown_(e: KeyboardEvent) {
    // Only handle "Escape" when in an active state.
    if (e.key !== 'Escape' || this.state_ === TextBoxState.INACTIVE) {
      return;
    }

    const target = e.composedPath()[0];
    if (target === this.$.textbox) {
      this.focus();
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

    // Delete key not in the textbox deletes the annotation.
    if (e.key === 'Delete') {
      this.textValue_ = '';
      this.commitTextAnnotation();
      return;
    }

    // Ignore all other keys except arrows. Also ignore if the user is already
    // dragging with the pointer.
    if (!['ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight'].includes(e.key) ||
        this.pointerStart_ !== null) {
      return;
    }

    // Reset state if this is a new arrow key.
    if (this.currentArrowKey_ !== null && this.currentArrowKey_ !== e.key) {
      this.onHandleKeyUp_();
    }
    this.currentArrowKey_ = e.key;

    if (this.keyDownCount_ === -1) {
      this.dragTarget_ = target;
      this.eventTracker_.add(target, 'keyup', () => this.onHandleKeyUp_());
      this.eventTracker_.add(target, 'focusout', () => this.onHandleKeyUp_());
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
    }
    this.onMove_(target, moveX, moveY);
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
      const maxDeltaX = this.pageX_ + this.pageWidth_ -
          (this.startPosition_.locationX + this.startPosition_.width);
      const deltaX = Math.min(
          maxDeltaX,
          Math.max(moveX, -1 * this.startPosition_.width + this.minWidth_));
      this.width_ = this.startPosition_.width + deltaX;
    }
    if (target.classList.contains('top')) {
      const deltaY = Math.max(
          this.pageY_ - this.startPosition_.locationY,
          Math.min(moveY, this.startPosition_.height - this.minHeight_));
      this.locationY_ = this.startPosition_.locationY + deltaY;
      this.height_ = this.startPosition_.height - deltaY;
    } else if (target.classList.contains('bottom')) {
      const maxDeltaY = this.pageHeight_ + this.pageY_ -
          (this.startPosition_.locationY + this.startPosition_.height);
      const deltaY = Math.min(
          maxDeltaY,
          Math.max(moveY, -1 * this.startPosition_.height + this.minHeight_));
      this.height_ = this.startPosition_.height + deltaY;
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
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-box': InkTextBoxElement;
  }
}

customElements.define(InkTextBoxElement.is, InkTextBoxElement);

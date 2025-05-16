// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes, TextBoxRect} from '../constants.js';
import {colorsEqual, convertRotatedCoordinates, Ink2Manager, stylesEqual} from '../ink2_manager.js';
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

// This is 12px of padding + 24px. For some reason, Blink crashes at < 24px wide
// textarea. Since the textarea won't resize width-wise automatically, it also
// doesn't work to set this dynamically like we do with the height; just set a
// reasonable minimum width regardless of the content of the text box. Note that
// this value is held constant regardless of zoom due to the rendering issue.
const MIN_WIDTH_PX = 36;

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

  // Note: locationX_, locationY_, minHeight_, height_ and width_ are in
  // screen coordinates.
  private accessor locationX_: number = 0;
  private accessor locationY_: number = 0;
  private accessor minHeight_: number = 0;
  private accessor height_: number = 0;
  private accessor state_: TextBoxState = TextBoxState.INACTIVE;
  private accessor textOrientation_: number = 0;
  protected accessor textRotations_: number = 0;
  protected accessor textValue_: string = '';
  private accessor viewportRotations_: number = 0;
  private accessor width_: number = 0;
  private accessor zoom_: number = 1.0;

  private attributes_?: TextAttributes;
  private eventTracker_: EventTracker = new EventTracker();
  // Whether this is an existing textbox. Tracked so that the textbox can
  // correctly notify the backend about changes (e.g. deleting all text in an
  // existing annotation should remove it from the PDF, so we need to commit
  // this change where we wouldn't commit an empty new annotation).
  private existing_: boolean = false;
  private id_: number = -1;
  private pageNumber_: number = -1;
  private pageX_: number = 0;
  private pageY_: number = 0;
  private pointerStart_: {x: number, y: number}|null = null;
  private startPosition_: TextBoxRect|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'initialize-text-box',
        (e: Event) =>
            this.onInitializeTextBox_((e as CustomEvent<TextBoxInit>).detail));
    this.onViewportChanged_(Ink2Manager.getInstance().getViewportParams());
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'blur-text-box',
        () => this.onBlurTextBox_());
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'viewport-changed',
        (e: Event) =>
            this.onViewportChanged_((e as CustomEvent<ViewportParams>).detail));
    this.eventTracker_.add(
        this, 'pointerdown', (e: PointerEvent) => this.onPointerDown_(e));
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
      this.height_ = Math.max(this.height_, this.minHeight_);
    }

    if (changedPrivateProperties.has('width_')) {
      const lastWidth =
          changedPrivateProperties.get('width_') as number | undefined;
      if (lastWidth !== undefined && lastWidth < this.width_) {
        // Reset the minimum height to 0 here, because it will have changed due
        // to the increase in width and needs to be recomputed.
        this.minHeight_ = 0;
      }
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
      this.style.setProperty('--textbox-width', `${this.width_}px`);
    }
    if (changedPrivateProperties.has('height_')) {
      this.style.setProperty('--textbox-height', `${this.height_}px`);
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
    if (changedPrivateProperties.has('width_') ||
        changedPrivateProperties.has('height_')) {
      this.updateMinimumHeight_();
    }
  }

  private styleFontSize_() {
    if (this.attributes_) {
      this.$.textbox.style.fontSize = `${this.attributes_.size * this.zoom_}px`;
    }
  }

  private onBlurTextBox_() {
    this.$.textbox.blur();
  }

  protected onTextareaFocus_() {
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
    this.updateMinimumHeight_();
  }

  private textBoxEdited_() {
    if (this.state_ === TextBoxState.NEW) {
      this.state_ = TextBoxState.EDITED;
    }
  }

  private updateMinimumHeight_() {
    if (this.$.textbox.scrollHeight > this.$.textbox.clientHeight) {
      this.minHeight_ = this.$.textbox.scrollHeight;
    } else {
      this.minHeight_ = Math.min(this.minHeight_, this.$.textbox.clientHeight);
    }
  }

  commitTextAnnotation() {
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
    this.pageX_ = data.pageCoordinates.x;
    this.pageY_ = data.pageCoordinates.y;
    this.width_ = data.annotation.textBoxRect.width;
    this.height_ = data.annotation.textBoxRect.height;
    this.minHeight_ = 0;
    this.locationX_ = data.annotation.textBoxRect.locationX;
    this.locationY_ = data.annotation.textBoxRect.locationY;
    this.state_ = TextBoxState.NEW;
    this.existing_ = data.annotation.text !== '';
    this.textValue_ =
        data.annotation.text === '' ? 'Sample Text' : data.annotation.text;
    this.id_ = data.annotation.id;
    this.pageNumber_ = data.annotation.pageNumber;
    this.textOrientation_ = data.annotation.textOrientation;
    this.updateTextAttributes_(data.annotation.textAttributes);
  }

  private onViewportChanged_(update: ViewportParams) {
    // Convert width, height, locationX, locationY to the new screen
    // coordinates.

    // Note that this.pageX_ and this.pageY_ are in the old screen
    // coordinates, i.e. they were using the old zoom value.
    const adjusted = {
      locationX: (this.locationX_ - this.pageX_) * update.zoom / this.zoom_,
      locationY: (this.locationY_ - this.pageY_) * update.zoom / this.zoom_,
      width: Math.max(this.width_ * update.zoom / this.zoom_, MIN_WIDTH_PX),
      height: this.height_ * update.zoom / this.zoom_,
    };
    const rotated = convertRotatedCoordinates(
        adjusted, this.viewportRotations_, update.clockwiseRotations,
        update.pageDimensions.width, update.pageDimensions.height);
    this.locationX_ = rotated.locationX + update.pageDimensions.x;
    this.locationY_ = rotated.locationY + update.pageDimensions.y;
    this.width_ = rotated.width;
    this.height_ = rotated.height;

    // Update properties to the new values.
    this.viewportRotations_ = update.clockwiseRotations;
    this.zoom_ = update.zoom;
    this.pageX_ = update.pageDimensions.x;
    this.pageY_ = update.pageDimensions.y;
  }

  protected onPointerDown_(e: PointerEvent) {
    const target = e.composedPath()[0];
    // Ignore pointer events on the textbox itself.
    if (e.button !== 0 || !(target instanceof HTMLElement) ||
        target === this.$.textbox) {
      return;
    }

    this.pointerStart_ = {x: e.x, y: e.y};
    this.startPosition_ = {
      locationX: this.locationX_,
      locationY: this.locationY_,
      width: this.width_,
      height: this.height_,
    };

    this.eventTracker_.add(
        target, 'pointercancel',
        (e: PointerEvent) => this.onHandlePointerUp_(e));
    this.eventTracker_.add(
        target, 'pointerup', (e: PointerEvent) => this.onHandlePointerUp_(e));
    this.eventTracker_.add(
        target, 'pointermove',
        (e: PointerEvent) => this.onHandlePointerMove_(e));
    target.setPointerCapture(e.pointerId);
  }

  private onHandlePointerMove_(e: PointerEvent) {
    const target = e.target as HTMLElement;
    assert(this.pointerStart_);
    assert(this.startPosition_);
    if (!target.classList.contains('handle')) {
      // User is dragging the box itself.
      const deltaX = e.x - this.pointerStart_.x;
      const deltaY = e.y - this.pointerStart_.y;
      this.locationX_ = this.startPosition_.locationX + deltaX;
      this.locationY_ = this.startPosition_.locationY + deltaY;
      return;
    }

    if (target.classList.contains('left')) {
      const deltaX = Math.min(
          e.x - this.pointerStart_.x, this.startPosition_.width - MIN_WIDTH_PX);
      this.locationX_ = this.startPosition_.locationX + deltaX;
      this.width_ = this.startPosition_.width - deltaX;
    } else if (target.classList.contains('right')) {
      const deltaX = Math.max(
          e.x - this.pointerStart_.x,
          -1 * this.startPosition_.width + MIN_WIDTH_PX);
      this.width_ = this.startPosition_.width + deltaX;
    }
    if (target.classList.contains('top')) {
      const deltaY = Math.min(
          e.y - this.pointerStart_.y,
          this.startPosition_.height - this.minHeight_);
      this.height_ = this.startPosition_.height - deltaY;
      this.locationY_ = this.startPosition_.locationY + deltaY;
    } else if (target.classList.contains('bottom')) {
      const deltaY = Math.max(
          e.y - this.pointerStart_.y,
          -1 * this.startPosition_.height + this.minHeight_);
      this.height_ = this.startPosition_.height + deltaY;
    }
  }

  private onHandlePointerUp_(e: PointerEvent) {
    const target = e.target as HTMLElement;
    this.pointerStart_ = null;
    this.startPosition_ = null;
    this.eventTracker_.remove(target, 'pointercancel');
    this.eventTracker_.remove(target, 'pointerup');
    this.eventTracker_.remove(target, 'pointermove');
    this.textBoxEdited_();
  }

  private updateTextAttributes_(newAttributes: TextAttributes) {
    this.$.textbox.style.fontFamily = newAttributes.typeface;
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
      this.updateMinimumHeight_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-box': InkTextBoxElement;
  }
}

customElements.define(InkTextBoxElement.is, InkTextBoxElement);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assert, assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation, TextAttributes, TextBoxRect} from '../constants.js';
import {TextStyle, TextTypeface} from '../constants.js';
import {Ink2Manager, MIN_TEXTBOX_SIZE_PX, stylesEqual} from '../ink2_manager.js';
import {convertRotatedCoordinates, screenToPageCoordinates} from '../ink_text_annotation_utils.js';
import {record, UserAction} from '../metrics.js';
import {PdfViewerPrivateProxyImpl} from '../pdf_viewer_private_proxy.js';
import {colorsEqual, colorToHex, hasCtrlModifier, hasCtrlModifierOnly, isStrikethroughShortcut} from '../pdf_viewer_utils.js';
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

function isArrowKey(key: string|null): boolean {
  return key !== null &&
      ['ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight'].includes(key);
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
      isPaste: {type: Boolean},
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
  accessor isPaste: boolean = false;
  accessor viewport: Viewport|null = null;
  accessor annotation: TextAnnotation|null = null;
  accessor pageDimensions: ViewportRect|null = null;

  private activeKey_: string|null = null;
  private arrowKeyDownCount_: number = -1;
  private attributes_?: TextAttributes;
  private dragTarget_: HTMLElement|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  // Whether this is an existing textbox. Tracked so that the textbox can
  // correctly notify the backend about changes (e.g. deleting all text in an
  // existing annotation should remove it from the PDF, so this change must be
  // committed where an empty new annotation would not be committed).
  private existing_: boolean = false;
  private id_: number = -1;
  private pageIndex_: number = -1;
  private pageHeight_: number = 0;
  private pageWidth_: number = 0;
  private pageX_: number = 0;
  private pageY_: number = 0;
  private pointerStart_: {x: number, y: number}|null = null;
  private promiseResolver_: PromiseResolver<void>|null = null;
  private startPosition_: TextBoxRect|null = null;

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

  // Populates a `TextAnnotation` with the current state that can
  // be passed to Ink2Manager.
  private createClipboardAnnotation_(): TextAnnotation|null {
    if (!this.viewport || !this.attributes_ || this.textValue_ === '') {
      return null;
    }
    const pageRect = screenToPageCoordinates(
        this.pageIndex_, {
          height: this.height_,
          locationX: this.locationX_,
          locationY: this.locationY_,
          width: this.width_,
        },
        this.viewport);
    return {
      id: this.id_,
      mojoTextInfo: new ArrayBuffer(0),
      pageIndex: this.pageIndex_,
      pdfZoom: this.zoom_,
      text: this.textValue_,
      textAttributes: structuredClone(this.attributes_),
      textBoxRect: pageRect,
      textOrientation: this.textOrientation_,
      viewportOrientation: this.viewportRotations_,
    };
  }

  private copyAnnotation_() {
    const annotation = this.createClipboardAnnotation_();
    if (!annotation) {
      return;
    }
    Ink2Manager.getInstance().saveAnnotationToClipboard(
        annotation, /*isCut=*/ false);
  }

  private cutAnnotation_() {
    const annotation = this.createClipboardAnnotation_();
    if (!annotation) {
      return;
    }
    Ink2Manager.getInstance().saveAnnotationToClipboard(
        annotation, /*isCut=*/ true);
    this.textValue_ = '';
    this.$.textbox.value = '';
    this.textBoxEdited_();
    this.commitTextAnnotation();
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
    this.activeKey_ = null;
    this.arrowKeyDownCount_ = -1;
  }

  private addKeyDragListeners_() {
    this.dragTarget_ = this;
    this.startPosition_ = {
      locationX: this.locationX_,
      locationY: this.locationY_,
      width: this.width_,
      height: this.height_,
    };
    this.eventTracker_.add(this, 'keyup', () => this.onHandleKeyUp_());
    this.eventTracker_.add(this, 'focusout', () => this.onHandleKeyUp_());
  }

  // Removes any drag listeners and resets location and dimensions to the start
  // position.
  private resetDrag_() {
    if (this.dragTarget_ === null) {
      return;
    }

    assert(this.startPosition_);
    this.locationX_ = this.startPosition_.locationX;
    this.locationY_ = this.startPosition_.locationY;
    this.width_ = this.startPosition_.width;
    this.height_ = this.startPosition_.height;
    this.startPosition_ = null;

    if (this.pointerStart_ !== null) {
      this.removePointerDragListeners_();
    } else if (this.activeKey_ !== null) {
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

    const hasTextValue = this.textValue_ !== '';
    const isUnedited = this.state_ !== TextBoxState.EDITED && !this.isPaste;
    if ((!hasTextValue || isUnedited) && !this.existing_) {
      // Empty textbox.
      this.finishCommit_();
      record(UserAction.ADD_INK2_TEXT_ANNOTATION_ABORTED);
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
      record(UserAction.EDIT_INK2_TEXT_ANNOTATION_ABORTED);
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
        let action;
        if (this.existing_) {
          action = hasTextValue ?
              UserAction.EDIT_INK2_TEXT_ANNOTATION :
              action = UserAction.DELETE_INK2_TEXT_ANNOTATION;
        } else {
          action = UserAction.ADD_INK2_TEXT_ANNOTATION;
        }
        record(action);
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
    this.state_ = this.isPaste ? TextBoxState.EDITED : TextBoxState.NEW;
    this.existing_ = annotation.text !== '' && !this.isPaste;
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
      if (this.isPaste) {
        this.focus();
      } else {
        this.$.textbox.focus();
      }
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
    if (this.state_ === TextBoxState.INACTIVE) {
      return;
    }

    // Handle text styling shortcuts.
    if (hasCtrlModifierOnly(e)) {
      const key = e.key.toLowerCase();
      if (key === 'b' || key === 'i') {
        e.preventDefault();
        e.stopPropagation();
        const style = key === 'b' ? TextStyle.BOLD : TextStyle.ITALIC;
        Ink2Manager.getInstance().toggleTextStyle(style);
        return;
      }
    }

    if (isStrikethroughShortcut(e)) {
      e.preventDefault();
      e.stopPropagation();
      Ink2Manager.getInstance().toggleTextStyle(TextStyle.STRIKETHROUGH);
      return;
    }

    const target = e.composedPath()[0];

    // Ignore keyboard events on the textbox itself, other than 'Escape', which
    // is separately handled by the global keyhandler above.
    if (!(target instanceof HTMLElement) || target === this.$.textbox) {
      return;
    }

    // Handle copy and cut.
    if (hasCtrlModifierOnly(e)) {
      const key = e.key.toLowerCase();
      if (key === 'c' || key === 'x') {
        e.preventDefault();
        e.stopPropagation();
        if (key === 'c') {
          this.copyAnnotation_();
        } else {
          this.cutAnnotation_();
        }
        return;
      }
    }

    // Backspace/Delete key not in the textbox deletes the annotation.
    if (e.key === 'Backspace' || e.key === 'Delete') {
      this.textValue_ = '';
      this.textBoxEdited_();
      this.commitTextAnnotation().then(() => {
        getAnnouncerInstance().announce(
            loadTimeData.getString('ink2TextAnnotationDeleted'));
      });
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
    if (!isArrowKey(e.key)) {
      return;
    }

    // Ignore arrow key dragging if keyboard resize shortcut is already active.
    if (this.activeKey_ !== null && !isArrowKey(this.activeKey_)) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    // Reset state if this is a new arrow key.
    if (this.activeKey_ !== null && this.activeKey_ !== e.key) {
      this.onHandleKeyUp_();
    }
    this.activeKey_ = e.key;

    if (this.arrowKeyDownCount_ === -1) {
      this.addKeyDragListeners_();
      this.arrowKeyDownCount_ = 0;
    }
    this.arrowKeyDownCount_++;

    let moveX = 0;
    let moveY = 0;
    switch (e.key) {
      case 'ArrowDown':
        moveY = this.arrowKeyDownCount_;
        break;
      case 'ArrowUp':
        moveY = -1 * this.arrowKeyDownCount_;
        break;
      case 'ArrowLeft':
        moveX = -1 * this.arrowKeyDownCount_;
        break;
      case 'ArrowRight':
        moveX = this.arrowKeyDownCount_;
        break;
      default:
        break;
    }
    this.onMove_(this, moveX, moveY);
  }

  private announceMoveOrResize_() {
    if (!this.startPosition_) {
      return;
    }

    if (this.width_ !== this.startPosition_.width ||
        this.height_ !== this.startPosition_.height) {
      getAnnouncerInstance().announce(
          loadTimeData.getString('ink2TextAnnotationResized'));
      return;
    }

    const deltaX = this.locationX_ - this.startPosition_.locationX;
    if (deltaX !== 0) {
      const stringId = deltaX > 0 ? 'ink2TextAnnotationMovedRight' :
                                    'ink2TextAnnotationMovedLeft';
      getAnnouncerInstance().announce(loadTimeData.getString(stringId));
    }

    const deltaY = this.locationY_ - this.startPosition_.locationY;
    if (deltaY !== 0) {
      const stringId = deltaY > 0 ? 'ink2TextAnnotationMovedDown' :
                                    'ink2TextAnnotationMovedUp';
      getAnnouncerInstance().announce(loadTimeData.getString(stringId));
    }
  }

  private onHandleKeyUp_() {
    this.announceMoveOrResize_();
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
    this.announceMoveOrResize_();
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
    this.$.textbox.style.textDecoration =
        newAttributes.styles.strikethrough ? 'line-through' : 'none';
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
    if (this.state_ === TextBoxState.INACTIVE || this.pointerStart_ !== null ||
        isArrowKey(this.activeKey_)) {
      return false;
    }

    const mainModifier = hasCtrlModifier(e);
    const secondModifier = isMac ? e.ctrlKey : e.altKey;

    if (!mainModifier || !secondModifier || e.shiftKey) {
      return false;
    }

    const key = e.key.toLowerCase();
    if (!['b', 'w', 'i', '9', 'k', 'j'].includes(key)) {
      return false;
    }

    if (this.activeKey_ !== null && this.activeKey_ !== key) {
      this.onHandleKeyUp_();
    }

    if (this.activeKey_ === null) {
      this.activeKey_ = key;
      this.addKeyDragListeners_();
    }

    switch (key) {
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
        assertNotReached();
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

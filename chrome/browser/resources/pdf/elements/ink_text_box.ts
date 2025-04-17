// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText, TextBoxRect} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import {colorToHex} from '../pdf_viewer_utils.js';

import {getCss} from './ink_text_box.css.js';
import {getHtml} from './ink_text_box.html.js';
import {InkTextObserverMixin} from './ink_text_observer_mixin.js';

export interface InkTextBoxElement {
  $: {
    textbox: HTMLTextAreaElement,
  };
}

// This is 12px of padding + 24px. For some reason, Blink crashes at < 24px wide
// textarea. Since the textarea won't resize width-wise automatically, it also
// doesn't work to set this dynamically like we do with the height; just set a
// reasonable minimum width regardless of the content of the text box.
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
      locationX_: {type: Number},
      locationY_: {type: Number},
      minHeight_: {type: Number},
      height_: {type: Number},
      textValue_: {type: String},
      width_: {type: Number},
    };
  }

  private accessor locationX_: number = 0;
  private accessor locationY_: number = 0;
  private accessor minHeight_: number = 0;
  private accessor height_: number = 0;
  protected accessor textValue_: string = 'Sample Text';
  private accessor width_: number = 0;

  private eventTracker_: EventTracker = new EventTracker();
  private pointerStart_: {x: number, y: number}|null = null;
  private sendTextboxUpdateTimeout_: number|null = null;
  private startPosition_: TextBoxRect|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'update-text-box',
        (e: Event) =>
            this.onUpdateTextBox_((e as CustomEvent<TextBoxRect>).detail));
    this.eventTracker_.add(
        this, 'pointerdown', (e: PointerEvent) => this.onPointerDown_(e));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
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

    if (changedPrivateProperties.has('width_') ||
        changedPrivateProperties.has('height_')) {
      this.hidden = this.width_ === 0 && this.height_ === 0;
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
    if (changedPrivateProperties.has('width_') ||
        changedPrivateProperties.has('height_')) {
      this.updateMinimumHeight_();
    }
  }

  protected onTextValueInput_() {
    this.textValue_ = this.$.textbox.value;
    this.updateMinimumHeight_();
    if (this.minHeight_ > this.height_) {
      // Height will adjust to minHeight_ on the next update cycle. Notify the
      // backend. Debouncing by 10ms.
      const update = {
        height: this.minHeight_,
        locationX: this.locationX_,
        locationY: this.locationY_,
        width: this.width_,
      };
      if (this.sendTextboxUpdateTimeout_) {
        clearTimeout(this.sendTextboxUpdateTimeout_);
      }
      this.sendTextboxUpdateTimeout_ = setTimeout(() => {
        this.sendTextboxUpdateTimeout_ = null;
        Ink2Manager.getInstance().setTextBoxRect(update);
      }, 10);
    }
  }

  private updateMinimumHeight_() {
    if (this.$.textbox.scrollHeight > this.$.textbox.clientHeight) {
      this.minHeight_ = this.$.textbox.scrollHeight;
    } else {
      this.minHeight_ = Math.min(this.minHeight_, this.$.textbox.clientHeight);
    }
  }

  private onUpdateTextBox_(update: TextBoxRect) {
    this.width_ = update.width;
    this.height_ = update.height;
    this.minHeight_ = 0;
    this.locationX_ = update.locationX;
    this.locationY_ = update.locationY;
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
    Ink2Manager.getInstance().setTextBoxRect({
      height: this.height_,
      locationX: this.locationX_,
      locationY: this.locationY_,
      width: this.width_,
    });
  }

  override onTextChanged(newTextStyles: AnnotationText) {
    this.$.textbox.style.fontFamily = newTextStyles.font;
    this.$.textbox.style.fontSize = `${newTextStyles.size}px`;
    this.$.textbox.style.textAlign = newTextStyles.alignment;
    this.$.textbox.style.fontStyle =
        newTextStyles.styles.italic ? 'italic' : 'normal';
    this.$.textbox.style.fontWeight =
        newTextStyles.styles.bold ? 'bold' : 'normal';
    let textDecoration = '';
    if (newTextStyles.styles.underline) {
      textDecoration += 'underline ';
    }
    if (newTextStyles.styles.strikethrough) {
      textDecoration += 'line-through';
    }
    this.$.textbox.style.textDecoration = textDecoration || 'none';
    this.$.textbox.style.color = colorToHex(newTextStyles.color);
    this.updateMinimumHeight_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-box': InkTextBoxElement;
  }
}

customElements.define(InkTextBoxElement.is, InkTextBoxElement);

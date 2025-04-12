// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText} from '../constants.js';
import type {TextBoxUpdate} from '../ink2_manager.js';
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
      height_: {type: Number},
      textValue_: {type: String},
      width_: {type: Number},
    };
  }

  private accessor locationX_: number = 0;
  private accessor locationY_: number = 0;
  private accessor height_: number = 0;
  protected accessor textValue_: string = 'Sample Text';
  private accessor width_: number = 0;

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        Ink2Manager.getInstance(), 'update-text-box',
        (e: Event) =>
            this.onUpdateTextBox_((e as CustomEvent<TextBoxUpdate>).detail));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
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
  }

  protected onTextValueChange_() {
    this.textValue_ = this.$.textbox.value;
  }

  private onUpdateTextBox_(update: TextBoxUpdate) {
    this.width_ = update.width;
    this.height_ = update.height;
    this.locationX_ = update.locationX;
    this.locationY_ = update.locationY;
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-box': InkTextBoxElement;
  }
}

customElements.define(InkTextBoxElement.is, InkTextBoxElement);

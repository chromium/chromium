// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText, TextStyles} from '../constants.js';
import {TextStyle} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import {getCss} from './text_styles_selector.css.js';
import {getHtml} from './text_styles_selector.html.js';


export class TextStylesSelectorElement extends CrLitElement {
  static get is() {
    return 'text-styles-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentStyles_: {type: Object},
    };
  }

  protected currentStyles_: TextStyles = {
    [TextStyle.BOLD]: false,
    [TextStyle.ITALIC]: false,
    [TextStyle.UNDERLINE]: false,
    [TextStyle.STRIKETHROUGH]: false,
  };

  private tracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    this.onTextChanged_(Ink2Manager.getInstance().getCurrentText());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.tracker_.add(
        Ink2Manager.getInstance(), 'text-changed',
        (e: Event) =>
            this.onTextChanged_((e as CustomEvent<AnnotationText>).detail));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tracker_.removeAll();
  }

  protected getTextStyles_(): TextStyle[] {
    return Object.values(TextStyle);
  }

  protected onStyleButtonClick_(e: Event) {
    const style = (e.target as HTMLElement).dataset['style'] as TextStyle;
    const newStyles = Object.assign({}, this.currentStyles_);
    newStyles[style] = !newStyles[style];
    Ink2Manager.getInstance().setTextStyles(newStyles);
  }

  protected getActiveClass_(style: TextStyle) {
    return this.currentStyles_[style] ? 'active' : '';
  }

  protected getAriaPressed_(style: TextStyle) {
    return this.currentStyles_[style] ? 'true' : 'false';
  }

  private onTextChanged_(text: AnnotationText) {
    this.currentStyles_ = text.styles;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-styles-selector': TextStylesSelectorElement;
  }
}

customElements.define(TextStylesSelectorElement.is, TextStylesSelectorElement);

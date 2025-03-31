// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText, TextStyles} from '../constants.js';
import {TextAlignment, TextStyle} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

import {getCss} from './viewer_text_side_panel.css.js';
import {getHtml} from './viewer_text_side_panel.html.js';

export class ViewerTextSidePanelElement extends CrLitElement {
  static get is() {
    return 'viewer-text-side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentAlignment_: {type: String},
      currentFont_: {type: String},
      currentSize_: {type: Number},
      currentStyles_: {type: Object},
      fonts_: {type: Array},
      sizes_: {type: Array},
    };
  }

  protected currentAlignment_: TextAlignment = TextAlignment.LEFT;
  protected currentFont_: string = '';
  protected currentSize_: number = 0;
  protected currentStyles_: TextStyles = {
    [TextStyle.BOLD]: false,
    [TextStyle.ITALIC]: false,
    [TextStyle.UNDERLINE]: false,
    [TextStyle.STRIKETHROUGH]: false,
  };

  protected fonts_ = [
    'Roboto',
    'Serif',
    'Sans',
    'Monospace',
  ];
  protected sizes_ =
      [6, 8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64, 72, 100];

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

  protected isSelectedFont_(font: string) {
    return font === this.currentFont_;
  }

  protected isSelectedSize_(size: number) {
    return size === this.currentSize_;
  }

  protected onFontChange_(e: Event) {
    const newValue = (e.target as HTMLSelectElement).value;
    Ink2Manager.getInstance().setTextFont(newValue);
  }

  protected onSizeChange_(e: Event) {
    const newValue = Number((e.target as HTMLSelectElement).value);
    Ink2Manager.getInstance().setTextSize(newValue);
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

  protected onSelectedAlignmentChanged_(e: CustomEvent<{value: string}>) {
    const newAlignment = e.detail.value as TextAlignment;
    Ink2Manager.getInstance().setTextAlignment(newAlignment);
  }

  private onTextChanged_(text: AnnotationText) {
    this.currentFont_ = text.font;
    this.currentSize_ = text.size;
    this.currentStyles_ = text.styles;
    this.currentAlignment_ = text.alignment;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-text-side-panel': ViewerTextSidePanelElement;
  }
}

customElements.define(
    ViewerTextSidePanelElement.is, ViewerTextSidePanelElement);

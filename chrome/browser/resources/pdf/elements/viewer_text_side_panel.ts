// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText} from '../constants.js';
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
      currentFont_: {type: String},
      currentSize_: {type: Number},
      fonts_: {type: Array},
      sizes_: {type: Array},
    };
  }

  protected currentFont_: string = '';
  protected currentSize_: number = 0;
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

  private onTextChanged_(text: AnnotationText) {
    this.currentFont_ = text.font;
    this.currentSize_ = text.size;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-text-side-panel': ViewerTextSidePanelElement;
  }
}

customElements.define(
    ViewerTextSidePanelElement.is, ViewerTextSidePanelElement);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import {getCss} from './ink_brush_selector.css.js';
import {getHtml} from './ink_brush_selector.html.js';

export const BRUSH_TYPES: AnnotationBrushType[] = [
  AnnotationBrushType.PEN,
  AnnotationBrushType.HIGHLIGHTER,
  AnnotationBrushType.ERASER,
];

export interface InkBrushSelectorElement {
  $: {
    eraser: HTMLElement,
    highlighter: HTMLElement,
    pen: HTMLElement,
  };
}

export class InkBrushSelectorElement extends CrLitElement {
  static get is() {
    return 'ink-brush-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentType_: {state: true, type: String},
    };
  }

  private currentType_: AnnotationBrushType = AnnotationBrushType.PEN;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentType_')) {
      this.onBrushChanged_();
    }
  }

  protected onBrushClick_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const newType = targetElement.dataset['brush'] as AnnotationBrushType;
    if (this.currentType_ === newType) {
      return;
    }

    this.currentType_ = newType;
  }

  protected getIcon_(type: AnnotationBrushType): string {
    const isCurrentType = this.isCurrentType_(type);
    switch (type) {
      case AnnotationBrushType.ERASER:
        return isCurrentType ? 'pdf:ink-eraser-fill' : 'pdf:ink-eraser';
      case AnnotationBrushType.HIGHLIGHTER:
        return isCurrentType ? 'pdf:ink-highlighter-fill' :
                               'pdf:ink-highlighter';
      case AnnotationBrushType.PEN:
        return isCurrentType ? 'pdf:ink-pen-fill' : 'pdf:ink-pen';
    }
  }

  protected isCurrentType_(type: AnnotationBrushType): boolean {
    return this.currentType_ === type;
  }

  private onBrushChanged_(): void {
    this.fire('ink-brush-change', {type: this.currentType_});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-brush-selector': InkBrushSelectorElement;
  }
}

customElements.define(InkBrushSelectorElement.is, InkBrushSelectorElement);

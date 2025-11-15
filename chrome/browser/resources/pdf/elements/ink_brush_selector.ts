// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './selectable_icon_button.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import {record, UserAction} from '../metrics.js';

import {getCss} from './ink_brush_selector.css.js';
import {getHtml} from './ink_brush_selector.html.js';
import type {SelectableIconButtonElement} from './selectable_icon_button.js';

export const BRUSH_TYPES: AnnotationBrushType[] = [
  AnnotationBrushType.PEN,
  AnnotationBrushType.HIGHLIGHTER,
  AnnotationBrushType.ERASER,
];

export interface InkBrushSelectorElement {
  $: {
    eraser: SelectableIconButtonElement,
    highlighter: SelectableIconButtonElement,
    pen: SelectableIconButtonElement,
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
      currentType: {
        notify: true,
        type: String,
      },
    };
  }

  accessor currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected onSelectedChanged_(e: CustomEvent<{value: string}>) {
    const newType = e.detail.value as AnnotationBrushType;
    if (newType === this.currentType) {
      // Don't record programmatic changes to metrics.
      return;
    }

    this.currentType = newType;
    switch (newType) {
      case AnnotationBrushType.ERASER:
        record(UserAction.SELECT_INK2_BRUSH_ERASER);
        break;
      case AnnotationBrushType.HIGHLIGHTER:
        record(UserAction.SELECT_INK2_BRUSH_HIGHLIGHTER);
        break;
      case AnnotationBrushType.PEN:
        record(UserAction.SELECT_INK2_BRUSH_PEN);
        break;
    }
  }

  protected getIcon_(type: AnnotationBrushType): string {
    const isCurrentType = type === this.currentType;
    switch (type) {
      case AnnotationBrushType.ERASER:
        return isCurrentType ? 'pdf-ink:ink-eraser-fill' : 'pdf-ink:ink-eraser';
      case AnnotationBrushType.HIGHLIGHTER:
        return isCurrentType ? 'pdf-ink:ink-highlighter-fill' :
                               'pdf-ink:ink-highlighter';
      case AnnotationBrushType.PEN:
        return isCurrentType ? 'pdf-ink:ink-pen-fill' : 'pdf-ink:ink-pen';
    }
  }

  protected getLabel_(type: AnnotationBrushType): string {
    switch (type) {
      case AnnotationBrushType.ERASER:
        return loadTimeData.getString('annotationEraser');
      case AnnotationBrushType.HIGHLIGHTER:
        return loadTimeData.getString('annotationHighlighter');
      case AnnotationBrushType.PEN:
        return loadTimeData.getString('annotationPen');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-brush-selector': InkBrushSelectorElement;
  }
}

customElements.define(InkBrushSelectorElement.is, InkBrushSelectorElement);

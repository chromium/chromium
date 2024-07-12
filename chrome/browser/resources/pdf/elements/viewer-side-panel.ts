// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush} from '../constants.js';
import {PluginController} from '../controller.js';

import {getHtml} from './viewer-side-panel.html.js';

export interface ViewerSidePanelElement {
  $: {
    eraser: HTMLElement,
    highlighter: HTMLElement,
    pen: HTMLElement,
  };
}

export class ViewerSidePanelElement extends CrLitElement {
  static get is() {
    return 'viewer-side-panel';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentType_: {type: Object},
    };
  }

  private pluginController_: PluginController = PluginController.getInstance();

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
    this.currentType_ = newType;
  }

  private onBrushChanged_(): void {
    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    const brush: AnnotationBrush = {
      type: this.currentType_,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    };

    this.pluginController_.setAnnotationBrush(brush);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-side-panel': ViewerSidePanelElement;
  }
}

customElements.define(ViewerSidePanelElement.is, ViewerSidePanelElement);

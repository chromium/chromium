// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './loading_state.css.js';
import {getHtml} from './loading_state.html.js';

const GAP_X = 8;
const BODY_INDENT = 8;
const COLUMN_WIDTH = 220;

interface Rect {
  y: number;
  width: number;
  height: number;
  rx: number;
  firstColumnOnly: boolean;
}

function getX(index: number, isHeader: boolean): number {
  if (index === 0) {
    return 0;
  }
  const headerStart = index * (COLUMN_WIDTH + GAP_X);
  return isHeader ? headerStart : headerStart + BODY_INDENT;
}

export interface LoadingStateElement {
  $: {
    clipPath: HTMLElement,
    loadingContainer: HTMLElement,
    gradientContainer: HTMLElement,
    svg: HTMLElement,
  };
}

export class LoadingStateElement extends CrLitElement {
  static get is() {
    return 'loading-state';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      columnCount: {type: Number},
      showGradient_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  columnCount: number = 0;
  protected showGradient_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;
  private rects_: Rect[] = [
    {y: 0, width: COLUMN_WIDTH, height: 38, rx: 8, firstColumnOnly: false},
    {y: 44, width: COLUMN_WIDTH, height: 130, rx: 9.6, firstColumnOnly: false},
    {y: 215, width: 91.2, height: 16.8, rx: 4.8, firstColumnOnly: false},
    {y: 279.2, width: 118, height: 20, rx: 4.8, firstColumnOnly: false},
    {y: 344, width: 194, height: 17, rx: 4.8, firstColumnOnly: false},
    {y: 369.2, width: 116, height: 17, rx: 4.8, firstColumnOnly: false},
    {y: 431, width: 178, height: 17, rx: 4.8, firstColumnOnly: false},
    {y: 493, width: 141, height: 17, rx: 4.8, firstColumnOnly: false},
    {y: 190, width: 79, height: 17, rx: 4.8, firstColumnOnly: true},
    {y: 254.2, width: 79, height: 17, rx: 4.8, firstColumnOnly: true},
    {y: 319.2, width: 79, height: 17, rx: 4.8, firstColumnOnly: true},
    {y: 406.2, width: 79, height: 17, rx: 4.8, firstColumnOnly: true},
    {y: 468.2, width: 79, height: 17, rx: 4.8, firstColumnOnly: true},
  ];

  override connectedCallback(): void {
    super.connectedCallback();
    this.resizeObserver_ = this.createResizeObserver_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('columnCount')) {
      this.generateLoadingUi_();
    }
  }

  // Generate the contents of the SVG here, rather than in the HTML. We can't
  // use .map or dynamic bindings in the HTML because SVG parts need to be
  // created with createElementNS to properly pass in attributes.
  private generateLoadingUi_() {
    while (this.$.clipPath.firstChild) {
      this.$.clipPath.removeChild(this.$.clipPath.lastChild!);
    }
    for (let columnIndex = 0; columnIndex < this.columnCount; columnIndex++) {
      this.rects_.forEach((rect, rectIndex) => {
        if (rect.firstColumnOnly && columnIndex > 0) {
          return;
        }
        const rectElement =
            document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        rectElement.setAttribute('x', `${getX(columnIndex, rectIndex <= 1)}`);
        rectElement.setAttribute('y', `${rect.y}`);
        rectElement.setAttribute('width', `${rect.width}`);
        rectElement.setAttribute('height', `${rect.height}`);
        rectElement.setAttribute('rx', `${rect.rx}`);
        this.$.clipPath.appendChild(rectElement);
      });
    }

    this.$.svg.setAttribute('width', `${this.svgWidth_}`);
  }

  private createResizeObserver_(): ResizeObserver {
    const observer = new ResizeObserver(() => {
      this.showGradient_ =
          this.svgWidth_ > this.$.gradientContainer.offsetWidth;
    });
    observer.observe(this.$.gradientContainer);
    return observer;
  }

  private get svgWidth_() {
    return this.columnCount === 0 ?
        0 :
        this.columnCount * (COLUMN_WIDTH + GAP_X) - GAP_X;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'loading-state': LoadingStateElement;
  }
}

customElements.define(LoadingStateElement.is, LoadingStateElement);

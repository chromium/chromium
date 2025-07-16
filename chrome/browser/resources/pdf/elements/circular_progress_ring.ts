// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './circular_progress_ring.css.js';
import {getHtml} from './circular_progress_ring.html.js';

const STROKE_DASHARRAY = 566;

function convertPercentageToStrokeDashOffset(percentage: number): string {
  const ratio = percentage / 100;
  const value = Math.round(STROKE_DASHARRAY * (1 - ratio));
  return `${value}px`;
}

export interface CircularProgressRingElement {
  $: {
    innerProgress: HTMLElement,
  };
}

export class CircularProgressRingElement extends CrLitElement {
  static get is() {
    return 'circular-progress-ring';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      strokeDashOffset: {type: String},
      value: {type: Number},
    };
  }

  accessor value: number = 0;
  protected accessor strokeDashOffset: string = `${STROKE_DASHARRAY}px`;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('value')) {
      assert(this.value >= 0 && this.value <= 100);
      this.strokeDashOffset = convertPercentageToStrokeDashOffset(this.value);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'circular-progress-ring': CircularProgressRingElement;
  }
}

customElements.define(
    CircularProgressRingElement.is, CircularProgressRingElement);

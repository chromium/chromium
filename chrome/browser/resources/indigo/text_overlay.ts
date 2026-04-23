// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './text_overlay.css.js';
import {getHtml} from './text_overlay.html.js';

const STEP_INTERVAL_MS: number = 3000;
const TOTAL_STEPS: number = 3;

export class IndigoTextOverlayElement extends CrLitElement {
  static get is() {
    return 'indigo-text-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentStep_: {type: Number},
    };
  }

  protected accessor currentStep_: number = 0;

  private stepTimer_: number|null = null;

  startSequence() {
    this.stopSequence();

    this.currentStep_ = 1;

    this.stepTimer_ = window.setInterval(() => {
      if (this.currentStep_ < TOTAL_STEPS) {
        this.currentStep_++;
      } else {
        this.clearStepTimer_();
      }
    }, STEP_INTERVAL_MS);
  }

  stopSequence() {
    this.clearStepTimer_();
    this.currentStep_ = 0;
  }

  private clearStepTimer_() {
    if (this.stepTimer_ !== null) {
      window.clearInterval(this.stepTimer_);
      this.stepTimer_ = null;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-text-overlay': IndigoTextOverlayElement;
  }
}

customElements.define(IndigoTextOverlayElement.is, IndigoTextOverlayElement);

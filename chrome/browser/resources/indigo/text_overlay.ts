// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './text_overlay.css.js';
import {getHtml} from './text_overlay.html.js';

const STEP_INTERVAL_MS: number = 3000;
const TOTAL_STEPS: number = 3;
const TEXT_ENTRY_DELAY_MS: number = 800;

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
      showIcon_: {type: Boolean},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.style.setProperty(
        '--indigo-text-entry-delay', `${TEXT_ENTRY_DELAY_MS}ms`);
  }

  protected accessor currentStep_: number = 0;
  protected accessor showIcon_: boolean = false;

  private stepTimer_: number|null = null;
  private entryTimeout_: number|null = null;

  startSequence() {
    this.stopSequence();

    this.showIcon_ = true;

    this.entryTimeout_ = window.setTimeout(() => {
      this.currentStep_ = 1;
      this.entryTimeout_ = null;

      this.stepTimer_ = window.setInterval(() => {
        if (this.currentStep_ < TOTAL_STEPS) {
          this.currentStep_++;
        } else {
          this.clearStepTimer_();
        }
      }, STEP_INTERVAL_MS);
    }, TEXT_ENTRY_DELAY_MS);
  }

  stopSequence() {
    this.clearStepTimer_();
    if (this.entryTimeout_ !== null) {
      window.clearTimeout(this.entryTimeout_);
      this.entryTimeout_ = null;
    }
    this.currentStep_ = 0;
    this.showIcon_ = false;
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

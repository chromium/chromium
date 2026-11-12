// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_lottie/cr_lottie.js';

import type {CrLottieElement} from 'chrome://resources/cr_components/cr_lottie/cr_lottie.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './text_overlay.css.js';
import {getHtml} from './text_overlay.html.js';

const STEP_INTERVAL_MS: number = 3000;
const TOTAL_STEPS: number = 3;
// Duration of the spark's entry animation. The steps appear once it ends.
const TEXT_ENTRY_DELAY_MS: number = 800;
// The Lottie takes over once the spark's entry animation has settled, so this
// must not be shorter than TEXT_ENTRY_DELAY_MS.
const LOTTIE_SWITCH_DELAY_MS: number = TEXT_ENTRY_DELAY_MS + 200;

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
      sequenceId_: {type: Number},
      useLottie_: {type: Boolean},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.style.setProperty(
        '--indigo-text-entry-delay', `${TEXT_ENTRY_DELAY_MS}ms`);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopSequence();
  }

  protected accessor currentStep_: number = 0;
  protected accessor showIcon_: boolean = false;
  // A new id per sequence makes the template create a fresh spark and
  // cr-lottie, even if a sequence restarts while one is showing.
  protected accessor sequenceId_: number = 0;
  protected accessor useLottie_: boolean = false;

  private stepTimer_: number|null = null;
  private entryTimeout_: number|null = null;
  private lottieTimeout_: number|null = null;
  // The Lottie is loaded behind the spark from the start, and only swapped in
  // once it's initialized, so there are no blank frames while it loads.
  private initializedLottie_: CrLottieElement|null = null;
  private lottieSwitchDue_: boolean = false;

  startSequence() {
    this.stopSequence();

    this.sequenceId_++;
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

    this.lottieTimeout_ = window.setTimeout(() => {
      this.lottieTimeout_ = null;
      this.lottieSwitchDue_ = true;
      this.maybeSwitchToLottie_();
    }, LOTTIE_SWITCH_DELAY_MS);
  }

  stopSequence() {
    this.clearStepTimer_();
    if (this.entryTimeout_ !== null) {
      window.clearTimeout(this.entryTimeout_);
      this.entryTimeout_ = null;
    }
    if (this.lottieTimeout_ !== null) {
      window.clearTimeout(this.lottieTimeout_);
      this.lottieTimeout_ = null;
    }
    this.initializedLottie_ = null;
    this.lottieSwitchDue_ = false;
    this.currentStep_ = 0;
    this.showIcon_ = false;
    this.useLottie_ = false;
  }

  protected onLottieCrLottieInitialized_(e: Event) {
    this.initializedLottie_ = e.target as CrLottieElement;
    this.maybeSwitchToLottie_();
  }

  private maybeSwitchToLottie_() {
    const lottie = this.shadowRoot.querySelector<CrLottieElement>('#lottie');
    if (!this.lottieSwitchDue_ || !lottie ||
        lottie !== this.initializedLottie_ || this.useLottie_) {
      return;
    }
    lottie.setPlay(true);
    this.useLottie_ = true;
  }

  private clearStepTimer_() {
    if (this.stepTimer_ !== null) {
      window.clearInterval(this.stepTimer_);
      this.stepTimer_ = null;
    }
  }

  protected getStepClass_(step: number): string {
    assert(step > 0);
    const previousStep = this.currentStep_ - 1;
    if (step === previousStep) {
      return 'wipe-out';
    }
    if (step === this.currentStep_) {
      return 'wipe-in';
    }
    return 'hidden';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-text-overlay': IndigoTextOverlayElement;
  }
}

customElements.define(IndigoTextOverlayElement.is, IndigoTextOverlayElement);

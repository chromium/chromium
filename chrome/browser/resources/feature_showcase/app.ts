// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';
import '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './feature_showcase_step.js';
import './example/example_step.js';
import './default_browser/default_browser_step.js';

import type {CrLottieElement} from '//resources/cr_elements/cr_lottie/cr_lottie.js';
import type {CrViewManagerElement} from '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface FeatureShowcaseAppElement {
  $: {
    viewManager: CrViewManagerElement,
    rightAnimation: CrLottieElement,
    bottomAnimation: CrLottieElement,
  };
}

export class FeatureShowcaseAppElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isDarkMode_: {type: Boolean},
    };
  }

  private activeStepIndex_: number = 0;
  private steps_: string[];
  protected accessor isDarkMode_: boolean = false;
  private matchMedia_: MediaQueryList;
  private darkModeListener_: (e: MediaQueryListEvent) => void;

  constructor() {
    super();
    const steps = new URLSearchParams(window.location.search).get('steps');
    this.steps_ = steps ?
        steps.split(',').map(s => s.trim()).filter(s => s.length > 0) :
        [];

    this.matchMedia_ = window.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = (e: MediaQueryListEvent) => {
      this.isDarkMode_ = e.matches;
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);
  }

  override firstUpdated() {
    // TODO(crbug.com/500274411): Clarify if assert here is ok or it's better
    // to have more graceful handling.
    assert(
        this.steps_.length > 0, 'Feature showcase requires at least one step.');

    const step = this.steps_[this.activeStepIndex_]!;
    this.$.viewManager.switchView(step, 'fade-in', 'fade-out');
  }

  protected getAnimationUrl_(position: 'right'|'bottom'): string {
    return `chrome://feature-showcase/animations/showcase_transition_${
        position}${this.isDarkMode_ ? '_dark' : ''}.json`;
  }

  protected hasStep_(stepId: string): boolean {
    return this.steps_.includes(stepId);
  }

  protected onStepCompleted_() {
    this.activeStepIndex_++;
    if (this.activeStepIndex_ < this.steps_.length) {
      this.tryPlayingTransitionAnimations();
      const step = this.steps_[this.activeStepIndex_]!;
      this.$.viewManager.switchView(step);
    }
    // TODO(crbug.com/507795442): Inform controller of showcase completion.
  }

  private tryPlayingTransitionAnimations() {
    assert(this.activeStepIndex_ > 0, 'Step index should be greater than 0.');
    const startFrame = (this.activeStepIndex_ - 1) * 120;
    const endFrame = startFrame + 120;

    this.$.rightAnimation.playSegments([startFrame, endFrame]);
    this.$.bottomAnimation.playSegments([startFrame, endFrame]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-app': FeatureShowcaseAppElement;
  }
}

customElements.define(FeatureShowcaseAppElement.is, FeatureShowcaseAppElement);

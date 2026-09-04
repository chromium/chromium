// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_components/cr_lottie/cr_lottie.js';
import '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/strings.m.js';
import './feature_showcase_step.js';
import './feature_showcase_stepper.js';
import './default_browser/default_browser_step.js';
import './gemini/gemini_step.js';
import './google_lens/google_lens_step.js';
import './themes_and_customization/themes_and_customization_step.js';
import './password_manager/password_manager_step.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrLottieElement} from '//resources/cr_components/cr_lottie/cr_lottie.js';
import type {CrViewManagerElement} from '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './feature_showcase.mojom-webui.js';

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
      activeStepIndex: {type: Number},
      steps: {type: Array},
      areButtonsDisabled_: {type: Boolean},
      isDarkMode_: {type: Boolean},
    };
  }

  accessor activeStepIndex: number = 0;
  accessor steps: string[] = [];
  protected accessor areButtonsDisabled_: boolean = false;
  protected accessor isDarkMode_: boolean = false;
  private matchMedia_: MediaQueryList;
  private darkModeListener_: (e: MediaQueryListEvent) => void;

  constructor() {
    super();
    const steps = new URLSearchParams(window.location.search).get('steps');
    this.steps = steps ?
        steps.split(',').map(s => s.trim()).filter(s => s.length > 0) :
        [];

    this.matchMedia_ = window.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode_ = this.matchMedia_.matches;
    this.darkModeListener_ = async (e: MediaQueryListEvent) => {
      this.isDarkMode_ = e.matches;
      await this.updateComplete;

      // Play a single frame to prevent lottie engine from exiting internal loop
      // early. When frames are equal, lottie thinks it has already reached the
      // end of the animation and sends 'complete' event, cancelling all pending
      // RequestAnimationFrame.
      // TODO(crbug.com/500662042): Consider exposing goToAndStop(...).
      const currentFrame = this.activeStepIndex * 120;
      this.$.rightAnimation.playSegments([currentFrame, currentFrame + 1]);
      this.$.bottomAnimation.playSegments([currentFrame, currentFrame + 1]);
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.matchMedia_.addEventListener('change', this.darkModeListener_);
    const updater = ColorChangeUpdater.forDocument();
    updater.start();
    // Force an initial refresh to avoid the race condition where the profile
    // theme loads after the page, but before the listener is ready.
    updater.refreshColorsCss();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.matchMedia_.removeEventListener('change', this.darkModeListener_);
  }

  override firstUpdated() {
    // TODO(crbug.com/500274411): Clarify if assert here is ok or it's better
    // to have more graceful handling.
    assert(
        this.steps.length > 0, 'Feature showcase requires at least one step.');

    const step = this.steps[this.activeStepIndex]!;
    this.$.viewManager.switchView(step).then(() => {
      this.notifyStepShown_();
    });
  }

  protected getAnimationUrl_(position: 'right'|'bottom'): string {
    return `chrome://feature-showcase/animations/showcase_transition_${
        position}${this.isDarkMode_ ? '_dark' : ''}.json`;
  }

  protected hasStep_(stepId: string): boolean {
    return this.steps.includes(stepId);
  }

  protected onStepCompleted_() {
    assert(!this.areButtonsDisabled_, 'Buttons should not be disabled.');
    this.areButtonsDisabled_ = true;
    this.activeStepIndex++;

    if (this.activeStepIndex < this.steps.length) {
      this.tryPlayingTransitionAnimations();
      const step = this.steps[this.activeStepIndex]!;
      this.$.viewManager.switchView(step).then(() => {
        this.areButtonsDisabled_ = false;
        this.notifyStepShown_();
      });
      return;
    }

    browserProxyFactory.getInstance().handler.finishFeatureShowcase();
  }

  private notifyStepShown_() {
    browserProxyFactory.getInstance().handler.nextStepShown();
  }

  private tryPlayingTransitionAnimations() {
    assert(this.activeStepIndex > 0, 'Step index should be greater than 0.');
    const startFrame = (this.activeStepIndex - 1) * 120;
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

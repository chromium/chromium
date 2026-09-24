// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_components/cr_lottie/cr_lottie.js';
import '//resources/cr_elements/icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './feature_showcase_stepper.css.js';
import {getHtml} from './feature_showcase_stepper.html.js';

const FeatureShowcaseStepperElementBase = I18nMixinLit(CrLitElement);

export class FeatureShowcaseStepperElement extends
    FeatureShowcaseStepperElementBase {
  static get is() {
    return 'feature-showcase-stepper';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      steps: {type: Array},
      activeIndex: {type: Number},
      completedAnimationIndex_: {type: Number},
      useStaticCheck_: {type: Boolean},
    };
  }

  accessor steps: string[] = [];
  accessor activeIndex: number = 0;

  private forcedColorsQuery_ = window.matchMedia('(forced-colors: active)');

  /**
   * Whether to force a static checkmark in place of the animation - either for
   * testing or for a11y reasons when forced colors mode is active.
   */
  protected accessor useStaticCheck_: boolean =
      loadTimeData.getBoolean('disableAnimations') ||
      this.forcedColorsQuery_.matches;

  /**
   * Steps are completed in order, so every index at or below this one is done.
   * These steps render a static checkmark.
   */
  protected accessor completedAnimationIndex_: number = -1;

  override connectedCallback() {
    super.connectedCallback();
    this.forcedColorsQuery_.addEventListener(
        'change', this.onForcedColorsChanged_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.forcedColorsQuery_.removeEventListener(
        'change', this.onForcedColorsChanged_);
  }

  private onForcedColorsChanged_ = (e: MediaQueryListEvent) => {
    if (e.matches) {
      this.useStaticCheck_ = true;
    }
  };

  /**
   * Only the step that just completed animates; any earlier one is already
   * static, so at most one animation runs at a time. Every operand is
   * monotonic, so a static checkmark never reverts to the animation.
   */
  protected shouldShowStaticCheck_(index: number): boolean {
    return this.useStaticCheck_ || index < this.activeIndex - 1 ||
        index <= this.completedAnimationIndex_;
  }

  async waitForAnimationComplete(): Promise<void> {
    await this.updateComplete;
    const animation = this.shadowRoot.querySelector('cr-lottie');
    if (this.useStaticCheck_ || !animation) {
      return;
    }

    await new Promise<void>(resolve => {
      const controller = new AbortController();
      const finish = () => {
        clearTimeout(timeoutId);
        controller.abort();
        resolve();
      };

      const timeoutId = setTimeout(finish, 1000);
      animation.addEventListener(
          'cr-lottie-completed', finish, {signal: controller.signal});
      this.forcedColorsQuery_.addEventListener(
          'change', finish, {signal: controller.signal});
    });
  }

  protected onCrLottieCompleted_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    if (Number.isNaN(index)) {
      return;
    }

    this.completedAnimationIndex_ =
        Math.max(this.completedAnimationIndex_, index);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-stepper': FeatureShowcaseStepperElement;
  }
}

customElements.define(
    FeatureShowcaseStepperElement.is, FeatureShowcaseStepperElement);

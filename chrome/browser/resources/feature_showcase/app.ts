// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './feature_showcase_step.js';
import './example/example_step.js';
import './default_browser/default_browser_step.js';

import type {CrViewManagerElement} from '//resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface FeatureShowcaseAppElement {
  $: {
    viewManager: CrViewManagerElement,
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

  private activeStepIndex_: number = 0;
  private steps_: string[];

  constructor() {
    super();
    const steps = new URLSearchParams(window.location.search).get('steps');
    this.steps_ = steps ?
        steps.split(',').map(s => s.trim()).filter(s => s.length > 0) :
        [];
  }

  override firstUpdated() {
    // TODO(crbug.com/500274411): Clarify if assert here is ok or it's better
    // to have more graceful handling.
    assert(
        this.steps_.length > 0, 'Feature showcase requires at least one step.');

    const step = this.steps_[this.activeStepIndex_]!;
    this.$.viewManager.switchView(step, 'fade-in', 'fade-out');
  }

  protected hasStep_(stepId: string): boolean {
    return this.steps_.includes(stepId);
  }

  protected onStepCompleted_() {
    this.activeStepIndex_++;
    if (this.activeStepIndex_ < this.steps_.length) {
      const step = this.steps_[this.activeStepIndex_]!;
      this.$.viewManager.switchView(step, 'fade-in', 'fade-out');
    }
    // TODO(crbug.com/507795442): Inform controller of showcase completion.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-app': FeatureShowcaseAppElement;
  }
}

customElements.define(FeatureShowcaseAppElement.is, FeatureShowcaseAppElement);

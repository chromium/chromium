// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {CrTooltipElement} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './onboarding_tooltip.css.js';
import {getHtml} from './onboarding_tooltip.html.js';

export class ContextualTasksOnboardingTooltipElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-onboarding-tooltip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      target: {type: Object},
      shouldShow: {type: Boolean},
    };
  }

  // The element that the tooltip is anchored to.
  accessor target: Element|null = null;
  accessor shouldShow: boolean = false;
  protected onboardingTitle_: string =
      loadTimeData.getString('onboardingTitle');
  protected onboardingBody_: string = loadTimeData.getString('onboardingBody');
  protected onboardingLink_: string = loadTimeData.getString('onboardingLink');
  protected onboardingLinkUrl_: string =
      loadTimeData.getString('onboardingLinkUrl');

  private get tooltip_(): CrTooltipElement {
    return this.shadowRoot.querySelector('cr-tooltip')!;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('target') && this.target) {
      this.tooltip_.target = this.target;
    }

    if (changedProperties.has('shouldShow') && !this.shouldShow) {
      this.hide();
    }
  }

  show() {
    if (this.tooltip_) {
      this.tooltip_.show();
    }
  }

  hide() {
    if (this.tooltip_) {
      this.tooltip_.hide();
    }
  }

  protected onTooltipClose_(e: Event) {
    e.stopPropagation();
    this.hide();
  }

  protected onHelpLinkClick_(e: Event) {
    e.preventDefault();
    BrowserProxyImpl.getInstance().handler.openOnboardingHelpUi();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-onboarding-tooltip':
        ContextualTasksOnboardingTooltipElement;
  }
}

customElements.define(
    ContextualTasksOnboardingTooltipElement.is,
    ContextualTasksOnboardingTooltipElement);

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

  updatePosition() {
    if (this.tooltip_ && this.target) {
      const targetRect = this.target.getBoundingClientRect();

      // Margin to the right of the window.
      const MARGIN_RIGHT = 24;
      const availableWidth =
          window.innerWidth - targetRect.left - MARGIN_RIGHT;
      this.tooltip_.style.setProperty(
          '--onboarding-tooltip-width', `${availableWidth}px`);

      const parentRect = this.tooltip_.offsetParent?.getBoundingClientRect();
      const tooltipRect = this.tooltip_.getBoundingClientRect();

      if (parentRect) {
        const left = targetRect.left - parentRect.left;
        // Position top: targetTop - tooltipHeight.
        // We use offset=0 as configured in HTML.
        const top = (targetRect.top - parentRect.top) - tooltipRect.height;

        this.tooltip_.style.left = `${left}px`;
        this.tooltip_.style.top = `${top}px`;
        this.tooltip_.style.right = 'auto';
      }
    }
  }

  protected onTooltipClose_(e: Event) {
    e.stopPropagation();
    BrowserProxyImpl.getInstance().handler.onboardingTooltipDismissed();
    this.hide();
    this.dispatchEvent(new CustomEvent('onboarding-tooltip-dismissed', {
      bubbles: true,
      composed: true,
    }));
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

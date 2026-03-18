// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
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

  private onboardingTooltipIsVisible_: boolean = false;
  private numberOfTimesTooltipShown_: number = 0;
  private readonly maximumTimesTooltipShown_: number = loadTimeData.getInteger(
      'composeboxShowOnboardingTooltipSessionImpressionCap');
  private isOnboardingTooltipDismissCountBelowCap_: boolean =
      loadTimeData.getBoolean('isOnboardingTooltipDismissCountBelowCap');
  private userDismissedTooltip_: boolean = false;
  private tooltipResizeObserver_: ResizeObserver|null = null;
  private tooltipMutationObserver_: MutationObserver|null = null;
  private tooltipImpressionTimer_: number|null = null;
  private readonly tooltipImpressionDelay_: number =
      loadTimeData.getInteger('composeboxShowOnboardingTooltipImpressionDelay');

  private get tooltip_(): CrTooltipElement {
    return this.shadowRoot.querySelector('cr-tooltip')!;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopObservingTooltipResize_();
    this.clearTooltipImpressionTimer_();
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
      this.updatePosition();
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

  updateTooltipVisibility(
      composeboxContainer: HTMLElement, composebox: ComposeboxElement) {
    if (!loadTimeData.getBoolean('showOnboardingTooltip')) {
      return;
    }

    if (this.onboardingTooltipIsVisible_ &&
        !composebox.getHasAutomaticActiveTabChipToken()) {
      this.hide();
      this.onboardingTooltipIsVisible_ = false;
      this.stopObservingTooltipResize_();
      this.clearTooltipImpressionTimer_();
    } else if (composebox.getHasAutomaticActiveTabChipToken()) {
      const target = composebox.getAutomaticActiveTabChipElement();
      if (target) {
        this.target = target;
      }

      if (this.onboardingTooltipIsVisible_) {
        this.updatePosition();
      } else if (this.shouldShowOnboardingTooltip()) {
        this.show();
        this.startObservingTooltipResize_(composeboxContainer, composebox, target);
        this.onboardingTooltipIsVisible_ = true;

        this.tooltipImpressionTimer_ = setTimeout(() => {
          this.numberOfTimesTooltipShown_++;
          this.tooltipImpressionTimer_ = null;
        }, this.tooltipImpressionDelay_);
      }
    }
    this.shouldShow = this.onboardingTooltipIsVisible_;
  }

  private shouldShowOnboardingTooltip(): boolean {
    return this.numberOfTimesTooltipShown_ < this.maximumTimesTooltipShown_ &&
        this.isOnboardingTooltipDismissCountBelowCap_ &&
        !this.userDismissedTooltip_;
  }

  private clearTooltipImpressionTimer_() {
    if (this.tooltipImpressionTimer_) {
      clearTimeout(this.tooltipImpressionTimer_);
      this.tooltipImpressionTimer_ = null;
    }
  }

  private startObservingTooltipResize_(
      composeboxContainer: HTMLElement, composebox: ComposeboxElement, target: Element|null) {
    this.stopObservingTooltipResize_();

    // Observe the tooltip for any size changes.
    this.tooltipResizeObserver_ = new ResizeObserver(() => {
      if (this.target) {
        this.updatePosition();
      }
    });
    this.tooltipResizeObserver_.observe(composebox);
    if (target) {
      this.tooltipResizeObserver_.observe(target);
    }

    // Observe the composebox container and composebox for any style or class
    // changes that may affect the tooltip position.
    this.tooltipMutationObserver_ = new MutationObserver(() => {
      if (this.target) {
        this.updatePosition();
      }
    });
    const mutationObserverOptions = {
      attributes: true,
      attributeFilter: ['style', 'class'],
    };
    this.tooltipMutationObserver_.observe(composeboxContainer, mutationObserverOptions);
    this.tooltipMutationObserver_.observe(composebox, mutationObserverOptions);
    if (target) {
      this.tooltipMutationObserver_.observe(target, mutationObserverOptions);
    }
  }

  private stopObservingTooltipResize_() {
    if (this.tooltipResizeObserver_) {
      this.tooltipResizeObserver_.disconnect();
      this.tooltipResizeObserver_ = null;
    }
    if (this.tooltipMutationObserver_) {
      this.tooltipMutationObserver_.disconnect();
      this.tooltipMutationObserver_ = null;
    }
  }

  protected onTooltipCloseClick_(e: Event) {
    e.stopPropagation();
    BrowserProxyImpl.getInstance().handler.onboardingTooltipDismissed();
    this.userDismissedTooltip_ = true;
    this.onboardingTooltipIsVisible_ = false;
    this.stopObservingTooltipResize_();
    this.clearTooltipImpressionTimer_();
    this.hide();
    this.fire('onboarding-tooltip-dismissed');
  }

  get numberOfTimesTooltipShownForTesting() {
    return this.numberOfTimesTooltipShown_;
  }

  set numberOfTimesTooltipShownForTesting(n: number) {
    this.numberOfTimesTooltipShown_ = n;
  }

  set userDismissedTooltipForTesting(dismissed: boolean) {
    this.userDismissedTooltip_ = dismissed;
  }

  get tooltipResizeObserverForTesting() {
    return this.tooltipResizeObserver_;
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

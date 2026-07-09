// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {ComposeboxEmbedderMixinInterface} from '//resources/cr_components/composebox/composebox_mixin.js';
import type {CrTooltipElement} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './lens_search_tooltip.css.js';
import {getHtml} from './lens_search_tooltip.html.js';

export class ContextualTasksLensSearchTooltipElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-lens-search-tooltip';
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

  accessor target: Element|null = null;
  accessor shouldShow: boolean = false;

  private lensSearchTooltipIsVisible_: boolean = false;
  private showCount_: number = 0;
  private userDismissed_: boolean = false;




  private tooltipResizeObserver_: ResizeObserver|null = null;
  private tooltipMutationObserver_: MutationObserver|null = null;

  private get tooltip_(): CrTooltipElement {
    return this.shadowRoot.querySelector('cr-tooltip')!;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopObservingTooltipResize_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('target') && this.target) {
      this.tooltip_.target = this.target;
    }
  }

  private shouldShowLensSearchTooltip(): boolean {
    return this.lensSearchTooltipIsVisible_ ||
        (!this.userDismissed_ &&
         loadTimeData.getBoolean('isLensSearchTooltipDismissCountBelowCap') &&
         !loadTimeData.getBoolean('isOnboardingTooltipDismissCountBelowCap') &&
         this.showCount_ <
             loadTimeData.getInteger('lensSearchTooltipSessionImpressionCap'));
  }


  show(composeboxContainer: HTMLElement, composebox: HTMLElement) {
    if (this.tooltip_) {
      this.tooltip_.show();
      this.updatePosition();
      this.startObservingTooltipResize_(composeboxContainer, composebox);
      this.lensSearchTooltipIsVisible_ = true;
      this.showCount_++;
      this.shouldShow = true;
    }
  }

  hide() {
    if (this.tooltip_) {
      this.tooltip_.hide();
      this.stopObservingTooltipResize_();
      this.lensSearchTooltipIsVisible_ = false;
      this.shouldShow = false;
    }
  }

  updateTooltipVisibility(
      composeboxContainer: HTMLElement,
      composebox: HTMLElement & ComposeboxEmbedderMixinInterface) {
    if (!this.shouldShowLensSearchTooltip()) {
      this.hide();
      return;
    }

    const lensButton = composebox.getLensButtonElement();
    if (lensButton) {
      this.target = lensButton;
      if (this.lensSearchTooltipIsVisible_) {
        this.updatePosition();
      } else {
        this.show(composeboxContainer, composebox);
      }
    } else if (this.lensSearchTooltipIsVisible_) {
      this.hide();
    }
    this.shouldShow = this.lensSearchTooltipIsVisible_;
  }


  updatePosition() {
    if (this.tooltip_ && this.target) {
      const targetRect = this.target.getBoundingClientRect();
      const MARGIN_LEFT = 24;
      const availableWidth = targetRect.right - MARGIN_LEFT;
      this.tooltip_.style.setProperty(
          '--lens-search-tooltip-width', `${availableWidth}px`);

      const parentRect = this.tooltip_.offsetParent?.getBoundingClientRect();
      const tooltipRect = this.tooltip_.getBoundingClientRect();

      if (parentRect) {
        const right = parentRect.right - targetRect.right;
        const top = (targetRect.top - parentRect.top) - tooltipRect.height;

        this.tooltip_.style.right = `${right}px`;
        this.tooltip_.style.top = `${top}px`;
        this.tooltip_.style.left = 'auto';
        this.tooltip_.style.bottom = 'auto';
      }
    }
  }


  private startObservingTooltipResize_(
      composeboxContainer: HTMLElement, composebox: HTMLElement) {
    this.stopObservingTooltipResize_();

    this.tooltipResizeObserver_ = new ResizeObserver(() => {
      if (this.target) {
        this.updatePosition();
      }
    });
    this.tooltipResizeObserver_.observe(composebox);
    if (this.target) {
      this.tooltipResizeObserver_.observe(this.target);
    }

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
    if (this.target) {
      this.tooltipMutationObserver_.observe(this.target, mutationObserverOptions);
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
    this.userDismissed_ = true;
    BrowserProxyImpl.getInstance().handler.lensSearchTooltipDismissed();
    this.hide();
    this.fire('lens-search-tooltip-dismissed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-lens-search-tooltip':
        ContextualTasksLensSearchTooltipElement;
  }
}

customElements.define(
    ContextualTasksLensSearchTooltipElement.is,
    ContextualTasksLensSearchTooltipElement);

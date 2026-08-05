// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {CrTooltipElement} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './info_tooltip.css.js';
import {getHtml} from './info_tooltip.html.js';

export class ContextualTasksInfoTooltipElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-info-tooltip';
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
      container: {type: Object},
      titleText: {type: String},
      bodyText: {type: String},
      closeButtonType: {
        type: String,
        reflect: true,
      },
      buttonText: {type: String},
      horizontalAlign: {
        type: String,
        reflect: true,
      },
      sideMargin: {type: Number},
      isCoinsEnabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor target: Element|null = null;
  accessor container: Element|null = null;
  accessor titleText: string = '';
  accessor bodyText: string = '';
  accessor closeButtonType: 'text'|'icon' = 'text';
  accessor buttonText: string = '';
  accessor horizontalAlign: 'left'|'right' = 'left';
  accessor sideMargin: number = 24;
  accessor isCoinsEnabled: boolean =
      loadTimeData.getBoolean('tabFaviconChipsToCoinsEnabled');

  private tooltipResizeObserver_: ResizeObserver|null = null;


  private get tooltip_(): CrTooltipElement {
    return this.shadowRoot.querySelector('cr-tooltip')!;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopObserving_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('target')) {
      if (this.target) {
        this.tooltip_.target = this.target;
        this.show();
        this.startObserving_();
      } else {
        this.hide();
        this.stopObserving_();
      }
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

  async updatePosition() {
    if (!this.tooltip_ || !this.target) {
      return;
    }

    await this.updateComplete;

    const targetRect = this.target.getBoundingClientRect();
    const parentRect = this.tooltip_.offsetParent?.getBoundingClientRect();
    const tooltipRect = this.tooltip_.getBoundingClientRect();

    if (this.horizontalAlign === 'left') {
      const availableWidth =
          window.innerWidth - targetRect.left - this.sideMargin;
      this.tooltip_.style.setProperty(
          '--info-tooltip-width', `${availableWidth}px`);
      if (parentRect) {
        const left = targetRect.left - parentRect.left;
        const top = (targetRect.top - parentRect.top) - tooltipRect.height;
        this.tooltip_.style.left = `${left}px`;
        this.tooltip_.style.top = `${top}px`;
        this.tooltip_.style.right = 'auto';
        this.tooltip_.style.bottom = 'auto';
      }
    } else {  // 'right'
      const availableWidth = targetRect.right - this.sideMargin;
      this.tooltip_.style.setProperty(
          '--info-tooltip-width', `${availableWidth}px`);
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

  private startObserving_() {
    this.stopObserving_();

    this.tooltipResizeObserver_ = new ResizeObserver(() => {
      this.updatePosition();
    });
    if (this.target) {
      this.tooltipResizeObserver_.observe(this.target);
    }
    if (this.container) {
      this.tooltipResizeObserver_.observe(this.container);
    }


  }

  private stopObserving_() {
    if (this.tooltipResizeObserver_) {
      this.tooltipResizeObserver_.disconnect();
      this.tooltipResizeObserver_ = null;
    }

  }

  protected onTooltipCloseClick_(e: Event) {
    e.stopPropagation();
    this.hide();
    this.fire('tooltip-dismissed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-info-tooltip': ContextualTasksInfoTooltipElement;
  }
}

customElements.define(
    ContextualTasksInfoTooltipElement.is, ContextualTasksInfoTooltipElement);

export class TooltipState {
  private showCount_ = 0;
  private userDismissed_ = false;
  private isActive_ = false;

  get showCount(): number {
    return this.showCount_;
  }

  get userDismissed(): boolean {
    return this.userDismissed_;
  }

  get isActive(): boolean {
    return this.isActive_;
  }

  constructor(
      private isFeatureEnabled: boolean,
      private isDismissCountBelowCap: boolean,
      private sessionImpressionCap: number,
      private dismissCallback: () => void) {}

  shouldShow(dependencyMet: boolean = true): boolean {
    if (this.userDismissed_) {
      return false;
    }
    if (!this.isFeatureEnabled) {
      return false;
    }
    if (!this.isDismissCountBelowCap) {
      return false;
    }

    if (this.isActive_) {
      return dependencyMet;
    }

    if (this.showCount_ >= this.sessionImpressionCap) {
      return false;
    }

    if (dependencyMet) {
      this.isActive_ = true;
      this.showCount_++;
    }

    return dependencyMet;
  }

  dismiss() {
    this.userDismissed_ = true;
    this.isActive_ = false;
    this.dismissCallback();
  }
}

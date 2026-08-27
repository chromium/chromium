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
import {EventTracker} from '//resources/js/event_tracker.js';
import {WindowOpenDisposition} from '//resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';

/**
 * Caret border width is 8px, producing a 16px wide triangle.
 * Subtracting half the caret width (8px) aligns its apex to the target center.
 */
const CARET_HALF_WIDTH_PX = 8;

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
      linkUrl: {type: String},
      linkText: {type: String},
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
  accessor linkUrl: string = '';
  accessor linkText: string = '';

  private tooltipResizeObserver_: ResizeObserver|null = null;
  private tooltipMutationObserver_: MutationObserver|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private updateScheduled_: boolean = false;

  private get tooltip_(): CrTooltipElement {
    return this.shadowRoot.querySelector('cr-tooltip')!;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopObserving_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('target') || changedProperties.has('container')) {
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

  /**
   * Batches position updates into a single animation frame to avoid
   * layout thrashing during typing or fast animations.
   */
  scheduleUpdatePosition() {
    if (this.updateScheduled_) {
      return;
    }
    this.updateScheduled_ = true;
    requestAnimationFrame(() => {
      this.updateScheduled_ = false;
      this.updatePosition();
    });
  }

  async updatePosition() {
    if (!this.tooltip_ || !this.target) {
      return;
    }

    await this.updateComplete;

    const targetRect = this.target.getBoundingClientRect();
    const parentRect = this.tooltip_.offsetParent?.getBoundingClientRect();
    const tooltipRect = this.tooltip_.getBoundingClientRect();

    if (!parentRect) {
      return;
    }

    this.updateVerticalPosition_(targetRect, parentRect, tooltipRect);

    if (this.horizontalAlign === 'left') {
      this.updateLeftAlignedPosition_(targetRect, parentRect);
    } else {
      this.updateRightAlignedPosition_(targetRect, parentRect);
    }
  }

  private updateVerticalPosition_(
      targetRect: DOMRect, parentRect: DOMRect, tooltipRect: DOMRect) {
    const top = (targetRect.top - parentRect.top) - tooltipRect.height;
    this.tooltip_.style.top = `${top}px`;
    this.tooltip_.style.bottom = 'auto';
  }

  private updateLeftAlignedPosition_(
      targetRect: DOMRect, parentRect: DOMRect) {
    const availableWidth =
        window.innerWidth - targetRect.left - this.sideMargin;
    this.tooltip_.style.setProperty(
        '--info-tooltip-width', `${availableWidth}px`);
    this.tooltip_.style.left = `${targetRect.left - parentRect.left}px`;
    this.tooltip_.style.right = 'auto';
  }

  private updateRightAlignedPosition_(
      targetRect: DOMRect, parentRect: DOMRect) {
    const availableWidth = targetRect.right - this.sideMargin;
    this.tooltip_.style.setProperty(
        '--info-tooltip-width', `${availableWidth}px`);
    this.tooltip_.style.right = `${parentRect.right - targetRect.right}px`;
    this.tooltip_.style.left = 'auto';

    this.updateCaretCenterOffset_(targetRect.width);
  }

  private updateCaretCenterOffset_(targetWidth: number) {
    const caretInlineEnd =
        Math.max(0, (targetWidth / 2) - CARET_HALF_WIDTH_PX);
    this.tooltip_.style.setProperty(
        '--info-tooltip-caret-inline-end', `${caretInlineEnd}px`);
  }

  private getTargetShadowHost_(): Element|null {
    const root = this.target?.getRootNode();
    return (root instanceof ShadowRoot) ? root.host : null;
  }

  private startObserving_() {
    this.stopObserving_();

    // 1. Observe size/viewport changes.
    this.tooltipResizeObserver_ = new ResizeObserver(() => {
      this.scheduleUpdatePosition();
    });
    if (this.target) {
      this.tooltipResizeObserver_.observe(this.target);
    }
    if (this.container) {
      this.tooltipResizeObserver_.observe(this.container);
    }

    // 2. Observe layout/state attribute changes on the host elements.
    this.tooltipMutationObserver_ = new MutationObserver(() => {
      this.scheduleUpdatePosition();
    });
    const mutationOptions: MutationObserverInit = {
      attributes: true,
      attributeFilter: ['style', 'class', 'submit-enabled', 'is-side-panel'],
    };

    const targetHost = this.getTargetShadowHost_();
    if (targetHost) {
      this.tooltipMutationObserver_.observe(targetHost, mutationOptions);
    }
    if (this.container) {
      this.tooltipMutationObserver_.observe(this.container, mutationOptions);
    }
    if (this.target) {
      this.tooltipMutationObserver_.observe(this.target, mutationOptions);
    }

    // 3. Lock in exact resting coordinates once the target button's CSS motion finishes.
    if (this.target) {
      this.eventTracker_.add(this.target, 'transitionend', () => {
        this.scheduleUpdatePosition();
      });
    }
  }

  private stopObserving_() {
    if (this.tooltipResizeObserver_) {
      this.tooltipResizeObserver_.disconnect();
      this.tooltipResizeObserver_ = null;
    }
    if (this.tooltipMutationObserver_) {
      this.tooltipMutationObserver_.disconnect();
      this.tooltipMutationObserver_ = null;
    }
    this.eventTracker_.removeAll();
    this.updateScheduled_ = false;
  }

  protected onLinkClick_(e: Event) {
    e.preventDefault();
    if (this.linkUrl) {
      BrowserProxyImpl.getInstance().handler.openUrl(
          this.linkUrl, WindowOpenDisposition.NEW_FOREGROUND_TAB);
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
      private isDismissCountBelowCap: boolean,
      private sessionImpressionCap: number,
      private dismissCallback: () => void) {}

  shouldShow(dependencyMet: boolean = true): boolean {
    if (this.userDismissed_) {
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './profile_tooltip.css.js';
import {getHtml} from './profile_tooltip.html.js';
import type {UnboundedElement} from './utils.js';

// Hover delays matching the OneGoogle Bar account tooltip (`ogb.ui.Tooltip`,
// which keeps the `goog.ui.Tooltip` 500ms show delay and sets a 100ms hide
// delay).
const SHOW_DELAY_MS = 500;
const HIDE_DELAY_MS = 100;

// Space between the tooltip and the anchor or the viewport edges.
const TOOLTIP_ANCHOR_GAP_PX = 4;
const TOOLTIP_VIEWPORT_MARGIN_PX = 8;

/** Returns the "Google Account" header shown at the top of the tooltip. */
function getGoogleAccountHeader(): string {
  // TODO(crbug.com/578985445): `googleAccount` temporarily reuses an existing
  // string; switch to a dedicated string.
  return loadTimeData.valueExists('googleAccount') ?
      loadTimeData.getString('googleAccount') :
      '';
}

export interface ProfileTooltipElement {
  $: {
    tooltip: UnboundedElement,
  };
}

/**
 * Hover tooltip for the toolbar profile avatar showing the Google Account
 * header, full name, and email. Uses the Unbounded Element API when available
 * so the tooltip can overflow the short toolbar WebView.
 */
export class ProfileTooltipElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-profile-tooltip';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fullName: {type: String},
      email: {type: String},
      open_: {type: Boolean},
      useUnbounded_: {type: Boolean},
    };
  }

  accessor fullName: string = '';
  accessor email: string = '';
  protected accessor open_: boolean = false;
  protected accessor useUnbounded_: boolean = false;
  private anchor_: HTMLElement|null = null;
  private showTimer_: number|null = null;
  private hideTimer_: number|null = null;

  get isOpen(): boolean {
    return this.open_;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.hide();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    // Recenter under the anchor when the content width changes while open.
    if (this.open_ && this.anchor_ &&
        (changedProperties.has('fullName') || changedProperties.has('email'))) {
      this.position_(this.anchor_);
    }
  }

  /**
   * Shows the tooltip below `anchor` after the hover delay. Cancels a pending
   * hide, so moving back onto the anchor keeps an open tooltip visible.
   */
  scheduleShow(anchor: HTMLElement) {
    this.clearHideTimer_();
    if (this.open_ || this.showTimer_ !== null) {
      return;
    }
    this.showTimer_ = setTimeout(() => {
      this.showTimer_ = null;
      this.show(anchor);
    }, SHOW_DELAY_MS);
  }

  /** Hides the tooltip after the hide delay and cancels a pending show. */
  scheduleHide() {
    this.clearShowTimer_();
    if (!this.open_ || this.hideTimer_ !== null) {
      return;
    }
    this.hideTimer_ = setTimeout(() => {
      this.hideTimer_ = null;
      this.hide();
    }, HIDE_DELAY_MS);
  }

  /** Shows the tooltip centered below `anchor` immediately. */
  async show(anchor: HTMLElement) {
    this.anchor_ = anchor;
    const useUnbounded = this.isUnboundedSupported_();
    this.useUnbounded_ = useUnbounded;
    this.open_ = true;
    await this.updateComplete;

    if (!this.open_ || !this.isConnected) {
      return;
    }

    this.position_(anchor);

    if (useUnbounded) {
      this.$.tooltip.showUnboundedElement?.().catch(err => {
        if (err instanceof DOMException && err.name === 'AbortError') {
          return;
        }
        this.useUnbounded_ = false;
      });
    }
  }

  /** Hides the tooltip immediately and cancels any pending show or hide. */
  hide(unboundedAlreadyDismissed: boolean = false) {
    this.clearShowTimer_();
    this.clearHideTimer_();
    if (!this.open_) {
      return;
    }
    this.open_ = false;
    this.anchor_ = null;
    if (this.useUnbounded_ && !unboundedAlreadyDismissed) {
      this.$.tooltip.hideUnboundedElement?.().catch(() => {});
    }
  }

  protected getHeader_(): string {
    return getGoogleAccountHeader();
  }

  protected onBeforetoggle_(e: Event) {
    if (!this.useUnbounded_) {
      return;
    }
    const toggleEvent = e as Event & {newState?: string};
    if (toggleEvent.newState === 'closed' && this.open_) {
      this.hide(/*unboundedAlreadyDismissed=*/ true);
    }
  }

  private clearShowTimer_() {
    if (this.showTimer_ !== null) {
      clearTimeout(this.showTimer_);
      this.showTimer_ = null;
    }
  }

  private clearHideTimer_() {
    if (this.hideTimer_ !== null) {
      clearTimeout(this.hideTimer_);
      this.hideTimer_ = null;
    }
  }

  private isUnboundedSupported_(): boolean {
    const flagEnabled =
        loadTimeData.valueExists('contextualTasksUnboundedMenuEnabled') &&
        loadTimeData.getBoolean('contextualTasksUnboundedMenuEnabled');
    if (!flagEnabled) {
      return false;
    }
    return typeof this.$.tooltip.showUnboundedElement === 'function';
  }

  private position_(anchor: HTMLElement) {
    const tooltip = this.$.tooltip;
    const rect = anchor.getBoundingClientRect();
    const tooltipWidth = tooltip.offsetWidth;
    const viewportWidth = document.documentElement.clientWidth;

    const desiredLeft = rect.left + (rect.width - tooltipWidth) / 2;
    const left = viewportWidth > 0 ?
        Math.max(
            TOOLTIP_VIEWPORT_MARGIN_PX,
            Math.min(
                desiredLeft,
                Math.max(
                    TOOLTIP_VIEWPORT_MARGIN_PX,
                    viewportWidth - tooltipWidth -
                        TOOLTIP_VIEWPORT_MARGIN_PX))) :
        Math.max(0, desiredLeft);
    const top = rect.bottom + TOOLTIP_ANCHOR_GAP_PX;

    tooltip.style.left = `${left}px`;
    tooltip.style.top = `${top}px`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-profile-tooltip': ProfileTooltipElement;
  }
}

customElements.define(ProfileTooltipElement.is, ProfileTooltipElement);

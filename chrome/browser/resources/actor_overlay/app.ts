// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Point} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import type {Theme} from './actor_overlay.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {ActorOverlayBrowserProxy} from './browser_proxy.js';

export interface ActorOverlayAppElement {
  $: {magicCursor: HTMLDivElement};
}

export class ActorOverlayAppElement extends CrLitElement {
  static get is() {
    return 'actor-overlay-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      borderGlowVisible_: {type: Boolean},
    };
  }

  protected accessor borderGlowVisible_: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();
  private listenerIds_: number[] = [];
  private shouldShowCursor_: boolean =
      loadTimeData.getBoolean('isMagicCursorEnabled');
  private isCursorInitialized_: boolean = false;
  private isStandaloneBorderGlowEnabled_: boolean =
      loadTimeData.getBoolean('isStandaloneBorderGlowEnabled');
  // Timer to start the loading state animation after cursor clicks and
  // movements.
  private loadingTimerId_: number|null = null;
  // Timer for window resize events.
  private resizeTimerId_: number|null = null;

  // Position State for Magic Cursor (Logical Pixels)
  private currentX_: number = 0;
  private currentY_: number = 0;
  // Speed the cursor maintains during animation, measured in pixels per
  // millisecond.
  private desiredSpeedPxPerMs_: number =
      Number(loadTimeData.getValue('magicCursorSpeed'));
  // The minimum allowed duration (in ms) for any single cursor movement.
  // Increasing this value makes short movements appear slower and smoother;
  // decreasing it makes them pop into position more instantly.
  private minDurationMs_: number =
      loadTimeData.getInteger('magicCursorMinDurationMs');
  // The maximum allowed duration (in ms) for any single cursor movement.
  // Increasing this value allows long movements to take more time; decreasing
  // it makes all long movements finish faster.
  private maxDurationMs_: number =
      loadTimeData.getInteger('magicCursorMaxDurationMs');

  override connectedCallback() {
    super.connectedCallback();
    const proxy = ActorOverlayBrowserProxy.getInstance();
    this.eventTracker_.add(this, 'pointerenter', () => {
      proxy.handler.onHoverStatusChanged(true);
    });
    this.eventTracker_.add(this, 'pointerleave', () => {
      proxy.handler.onHoverStatusChanged(false);
    });
    this.addEventListener('wheel', this.onWheelEvent_);
    this.eventTracker_.add(
        window, 'resize', this.handleWindowResize_.bind(this));
    this.listenerIds_ = [
      // Background scrim
      proxy.callbackRouter.setScrimBackground.addListener(
          this.setScrimBackground.bind(this)),
      // Border Glow
      proxy.callbackRouter.setBorderGlowVisibility.addListener(
          this.setBorderGlowVisibility.bind(this)),
      // Magic Cursor
      proxy.callbackRouter.moveCursorTo.addListener(
          this.moveCursorTo.bind(this)),
      proxy.callbackRouter.triggerClickAnimation.addListener(
          this.triggerClickAnimation.bind(this)),
      // Theme
      proxy.callbackRouter.setTheme.addListener(this.setTheme.bind(this)),
    ];
    proxy.handler.getCurrentBorderGlowVisibility().then(
        ({isVisible}) => this.setBorderGlowVisibility(isVisible));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.removeEventListener('wheel', this.onWheelEvent_);

    const proxy = ActorOverlayBrowserProxy.getInstance();
    this.listenerIds_.forEach(id => proxy.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
    if (this.loadingTimerId_) {
      clearTimeout(this.loadingTimerId_);
      this.loadingTimerId_ = null;
    }
    if (this.resizeTimerId_) {
      clearTimeout(this.resizeTimerId_);
      this.resizeTimerId_ = null;
    }
  }

  // Prevents user scroll gestures (mouse wheel, touchpad) from moving the
  // overlay.
  private onWheelEvent_(e: WheelEvent) {
    e.preventDefault();
    e.stopPropagation();
  }

  private handleWindowResize_() {
    if (!this.isCursorInitialized_) {
      return;
    }
    this.classList.add('is-resizing');
    if (this.resizeTimerId_) {
      clearTimeout(this.resizeTimerId_);
    }
    this.resizeTimerId_ = setTimeout(() => {
      this.classList.remove('is-resizing');
      this.resizeTimerId_ = null;
    }, 250);
  }

  private setScrimBackground(isVisible: boolean) {
    isVisible ? this.classList.add('background-visible') :
                this.classList.remove('background-visible');
  }

  private setBorderGlowVisibility(isVisible: boolean) {
    this.borderGlowVisible_ = this.isStandaloneBorderGlowEnabled_ && isVisible;
  }

  private setTheme(theme: Theme) {
    this.style.setProperty(
        '--actor-border-color', skColorToRgba(theme.borderColor));
    this.style.setProperty(
        '--actor-border-glow-color', skColorToRgba(theme.borderGlowColor));
    this.style.setProperty(
        '--actor-scrim-background-val1', skColorToRgba(theme.scrimColors[0]!));
    this.style.setProperty(
        '--actor-scrim-background-val2', skColorToRgba(theme.scrimColors[1]!));
    this.style.setProperty(
        '--actor-scrim-background-val3', skColorToRgba(theme.scrimColors[2]!));
    this.style.setProperty(
        '--actor-magic-cursor-filter',
        `drop-shadow(0px 3px 5px ${skColorToRgba(theme.magicCursorColor)})`);
  }

  private async triggerClickAnimation(): Promise<void> {
    const cursor = this.$.magicCursor;
    if (!cursor || !this.shouldShowCursor_ || !this.isCursorInitialized_) {
      return Promise.resolve();
    }

    if (this.loadingTimerId_) {
      clearTimeout(this.loadingTimerId_);
      this.loadingTimerId_ = null;
    }

    cursor.classList.remove('loading');
    cursor.style.setProperty('--cursor-x', `${Math.round(this.currentX_)}px`);
    cursor.style.setProperty('--cursor-y', `${Math.round(this.currentY_)}px`);

    return new Promise((resolve) => {
      const onAnimationEnd = () => {
        cursor.classList.remove('clicking');
        this.startLoadingTimer_();
        resolve();
      };
      // TODO(crbug.com/454339982): If the animationed event is never triggered,
      // we should resolve the callback with a false signal.
      cursor.addEventListener('animationend', onAnimationEnd, {once: true});
      cursor.classList.add('clicking');
    });
  }

  private moveCursorTo(point: Point): Promise<void> {
    if (!this.$.magicCursor || !this.shouldShowCursor_) {
      return Promise.resolve();
    }

    if (this.loadingTimerId_) {
      clearTimeout(this.loadingTimerId_);
      this.loadingTimerId_ = null;
    }

    this.$.magicCursor.classList.remove('loading');

    const scale = window.devicePixelRatio;
    const targetX = point.x / scale;
    const targetY = point.y / scale;

    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    // Initialize cursor position and state if first movement.
    if (!this.isCursorInitialized_) {
      this.$.magicCursor.style.opacity = '1';
      this.isCursorInitialized_ = true;
      // Check if the user prefers reduced motion to determine start position.
      if (prefersReducedMotion) {
        this.currentX_ = targetX;
        this.currentY_ = targetY;
      } else {
        // Initialize cursor at the top-left or top-right corner, whichever is
        // closer to the target.
        this.currentX_ =
            (targetX < window.innerWidth / 2) ? 0 : window.innerWidth;
        this.currentY_ = 0;
      }
      this.setCursorTransform(this.currentX_, this.currentY_);
      // Querying `offsetWidth` forces a page reflow to render the magic cursor
      // before calculating the cursor movement to the target.
      void this.$.magicCursor.offsetWidth;
    }

    // Resolve early if the visual position won't change, as no 'transitionend'
    // event will fire.
    if (Math.round(targetX) === Math.round(this.currentX_) &&
        Math.round(targetY) === Math.round(this.currentY_)) {
      this.currentX_ = targetX;
      this.currentY_ = targetY;
      return Promise.resolve();
    }

    // If reduced motion is enabled, skip the movement animation and update
    // coordinates of cursor instantly to the target position.
    if (prefersReducedMotion) {
      this.$.magicCursor.style.transitionDuration = '0ms';
      this.setCursorTransform(targetX, targetY);
      this.currentX_ = targetX;
      this.currentY_ = targetY;
      return Promise.resolve();
    }

    // Calculate distance and duration for animation
    const dx = targetX - this.currentX_;
    const dy = targetY - this.currentY_;
    const distance = Math.hypot(dx, dy);
    let durationMs = Math.round(distance / this.desiredSpeedPxPerMs_);
    durationMs = Math.max(
        this.minDurationMs_, Math.min(this.maxDurationMs_, durationMs));

    const transitionFinished = new Promise<void>(resolve => {
      // TODO(crbug.com/454339982): If the transitionend event is never
      // triggered, we should resolve the callback with a false signal.
      this.$.magicCursor.addEventListener('transitionend', () => {
        this.startLoadingTimer_();
        resolve();
      }, {once: true});
    });

    // Update transition duration based on the distance and apply the new cursor
    // position.
    this.$.magicCursor.style.transitionDuration = `${durationMs}ms`;
    this.setCursorTransform(targetX, targetY);

    // Update internal position state
    this.currentX_ = targetX;
    this.currentY_ = targetY;
    return transitionFinished;
  }

  private startLoadingTimer_() {
    if (this.loadingTimerId_) {
      clearTimeout(this.loadingTimerId_);
    }

    // Skip the loading animation when reduced motion is enabled.
    const prefersReducedMotion =
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    if (prefersReducedMotion) {
      return;
    }

    this.loadingTimerId_ = setTimeout(() => {
      if (this.$.magicCursor) {
        this.$.magicCursor.classList.add('loading');
      }
      this.loadingTimerId_ = null;
    }, 200);
  }

  private setCursorTransform(drawX: number, drawY: number) {
    this.$.magicCursor.style.transform =
        `translate(${Math.round(drawX)}px, ${Math.round(drawY)}px)`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'actor-overlay-app': ActorOverlayAppElement;
  }
}

customElements.define(ActorOverlayAppElement.is, ActorOverlayAppElement);

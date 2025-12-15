// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Point} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {ActorOverlayBrowserProxy} from './browser_proxy.js';

export interface ActorOverlayAppElement {
  $: {magicCursor: HTMLDivElement};
}

/**
 * Magic Cursor Kinematics Tuning Parameters for CSS Transitions.
 *
 * These constants define the cursor's movement (speed, duration, and
 * responsiveness). Adjusting these values controls the perceived pace and
 * smoothness of the cursor.
 */

// Constant speed the cursor maintains during animation, measured in pixels per
// millisecond.
const DESIRED_SPEED_PX_PER_MS = 0.667;
// The minimum allowed duration (in ms) for any single cursor movement.
// Increasing this value makes short movements appear slower and smoother;
// decreasing it makes them pop into position more instantly.
const MIN_DURATION_MS = 50;
// The maximum allowed duration (in ms) for any single cursor movement.
// Increasing this value allows long movements to take more time; decreasing it
// makes all long movements finish faster.
const MAX_DURATION_MS = 675;

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
  private setScrimBackgroundListenerId_: number | null = null;
  private setBorderGlowVisibilityListenerId_: number | null = null;
  private moveCursorToListenerId_: number|null = null;
  private shouldShowCursor_: boolean =
      loadTimeData.getBoolean('isMagicCursorEnabled');
  private isCursorInitialized_: boolean = false;
  private isStandaloneBorderGlowEnabled_: boolean =
      loadTimeData.getBoolean('isStandaloneBorderGlowEnabled');

  // Position State for Magic Cursor (Logical Pixels)
  private currentX_: number = 0;
  private currentY_: number = 0;

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

    // Background scrim
    this.setScrimBackgroundListenerId_ =
      proxy.callbackRouter.setScrimBackground.addListener(
        this.setScrimBackground.bind(this));

    // Border Glow
    this.setBorderGlowVisibilityListenerId_ =
      proxy.callbackRouter.setBorderGlowVisibility.addListener(
        this.setBorderGlowVisibility.bind(this));
    proxy.handler.getCurrentBorderGlowVisibility().then(
        ({isVisible}) => this.setBorderGlowVisibility(isVisible));

    // Magic Cursor
    this.moveCursorToListenerId_ =
        proxy.callbackRouter.moveCursorTo.addListener(
            this.moveCursorTo.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    this.removeEventListener('wheel', this.onWheelEvent_);

    assert(this.setScrimBackgroundListenerId_);
    ActorOverlayBrowserProxy.getInstance().callbackRouter.removeListener(
      this.setScrimBackgroundListenerId_);
    assert(this.setBorderGlowVisibilityListenerId_);
    ActorOverlayBrowserProxy.getInstance().callbackRouter.removeListener(
      this.setBorderGlowVisibilityListenerId_);
    assert(this.moveCursorToListenerId_);
    ActorOverlayBrowserProxy.getInstance().callbackRouter.removeListener(
        this.moveCursorToListenerId_);
  }

  // Prevents user scroll gestures (mouse wheel, touchpad) from moving the
  // overlay.
  private onWheelEvent_(e: WheelEvent) {
    e.preventDefault();
    e.stopPropagation();
  }

  private setScrimBackground(isVisible: boolean) {
    isVisible ? this.classList.add('background-visible') :
                this.classList.remove('background-visible');
  }

  private setBorderGlowVisibility(isVisible: boolean) {
    this.borderGlowVisible_ = this.isStandaloneBorderGlowEnabled_ && isVisible;
  }

  private moveCursorTo(point: Point): Promise<void> {
    if (!this.$.magicCursor || !this.shouldShowCursor_) {
      return Promise.resolve();
    }

    const scale = window.devicePixelRatio;
    const targetX = point.x / scale;
    const targetY = point.y / scale;
    // Get current position, using target if not initialized.
    const currentX = this.isCursorInitialized_ ? this.currentX_ : targetX;
    const currentY = this.isCursorInitialized_ ? this.currentY_ : targetY;

    // Initialize cursor position and state if first movement.
    if (!this.isCursorInitialized_) {
      this.$.magicCursor.style.opacity = '1';
      this.isCursorInitialized_ = true;
      this.setCursorTransform(targetX, targetY);
      this.currentX_ = targetX;
      this.currentY_ = targetY;
      // TODO(crbug.com/468313184): Determine if first coordinate should still
      // have a moving animation from a random location or if it should be set
      // initially without transition.
      return Promise.resolve();
    }

    // Calculate distance and duration for animation
    const dx = targetX - currentX;
    const dy = targetY - currentY;
    const distance = Math.hypot(dx, dy);
    let durationMs = Math.round(distance / DESIRED_SPEED_PX_PER_MS);
    durationMs =
        Math.max(MIN_DURATION_MS, Math.min(MAX_DURATION_MS, durationMs));

    const transitionFinished = new Promise<void>(resolve => {
      this.$.magicCursor.addEventListener('transitionend', () => {
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

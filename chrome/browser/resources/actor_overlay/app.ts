// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Point} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {getCss} from './app.css.js';
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
    return html`<div id="magicCursor"></div>`;
  }

  private eventTracker_: EventTracker = new EventTracker();
  private setHandoffButtonHoverStatusListenerId_: number|null = null;
  private shouldShowCursor_: boolean =
      loadTimeData.getBoolean('isMagicCursorEnabled');
  private isCursorInitialized_: boolean = false;
  private isHoveringOverlay_: boolean = false;
  private isHoveringButton_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    const proxy = ActorOverlayBrowserProxy.getInstance();
    this.eventTracker_.add(this, 'pointerenter', () => {
      this.isHoveringOverlay_ = true;
      proxy.handler.onHoverStatusChanged(true);
      this.updateScrimBackground();
    });
    this.eventTracker_.add(this, 'pointerleave', () => {
      this.isHoveringOverlay_ = false;
      proxy.handler.onHoverStatusChanged(false);
      this.updateScrimBackground();
    });
    this.setHandoffButtonHoverStatusListenerId_ =
        proxy.callbackRouter.setHandoffButtonHoverStatus.addListener(
            this.setHandoffButtonHoverStatus.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    assert(this.setHandoffButtonHoverStatusListenerId_);
    ActorOverlayBrowserProxy.getInstance().callbackRouter.removeListener(
        this.setHandoffButtonHoverStatusListenerId_);
  }

  private setHandoffButtonHoverStatus(isHovering: boolean) {
    this.isHoveringButton_ = isHovering;
    this.updateScrimBackground();
  }

  private updateScrimBackground() {
    const isHovering = this.isHoveringOverlay_ || this.isHoveringButton_;
    isHovering ? this.classList.add('background-visible') :
                 this.classList.remove('background-visible');
  }

  // TODO(crbug.com/422539773): Make function private once it's called via the
  // browser.
  moveCursorTo(point: Point) {
    if (!this.$.magicCursor || !this.shouldShowCursor_) {
      return;
    }
    if (!this.isCursorInitialized_) {
      this.$.magicCursor.style.opacity = '1';
      this.isCursorInitialized_ = true;
    }
    this.$.magicCursor.style.transform =
        `translate(${point.x}px, ${point.y}px)`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'actor-overlay-app': ActorOverlayAppElement;
  }
}

customElements.define(ActorOverlayAppElement.is, ActorOverlayAppElement);

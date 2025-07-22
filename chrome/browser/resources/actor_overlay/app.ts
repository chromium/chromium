// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {ActorOverlayBrowserProxy} from './browser_proxy.js';

export class ActorOverlayAppElement extends CrLitElement {
  static get is() {
    return 'actor-overlay-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return html``;
  }

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    const proxy = ActorOverlayBrowserProxy.getInstance();
    this.eventTracker_.add(this, 'pointerenter', () => {
      proxy.handler.onHoverStatusChanged(true);
    });
    this.eventTracker_.add(this, 'pointerleave', () => {
      proxy.handler.onHoverStatusChanged(false);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'actor-overlay-app': ActorOverlayAppElement;
  }
}

customElements.define(ActorOverlayAppElement.is, ActorOverlayAppElement);

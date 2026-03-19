// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './glic_selection_overlay.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {SelectionOverlayBaseHandler} from '/lens/selection_overlay_base_handler.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {DismissOverlayReason} from './selection_overlay.mojom-webui.js';
import {getCss} from './selection_overlay_app.css.js';
import {getHtml} from './selection_overlay_app.html.js';
import {SelectionOverlayBaseHandlerImpl} from './selection_overlay_base_handler_impl.js';

export class SelectionOverlayAppElement extends CrLitElement {
  static get is() {
    return 'selection-overlay-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor() {
    super();
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  // The screenshot of the viewport, received from the browser.
  protected screenshot_: BitmapMappedFromTrustedProcess|null = null;
  // Listener ids for events from the browser side.
  private listenerIds: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds =
        [this.browserProxy_.callbackRouter.screenshotReceived.addListener(
            (screenshot: BitmapMappedFromTrustedProcess) => {
              this.screenshot_ = screenshot;
            })];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy_.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  protected onCloseClick_() {
    this.browserProxy_.handler.dismissOverlay(
        DismissOverlayReason.kCloseButton);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'selection-overlay-app': SelectionOverlayAppElement;
  }
}

customElements.define(
    SelectionOverlayAppElement.is, SelectionOverlayAppElement);

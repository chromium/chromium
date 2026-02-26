// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {DismissOverlayReason} from './selection_overlay.mojom-webui.js';
import {getCss} from './selection_overlay_app.css.js';
import {getHtml} from './selection_overlay_app.html.js';

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

  private browserProxy_ = BrowserProxy.getInstance();
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

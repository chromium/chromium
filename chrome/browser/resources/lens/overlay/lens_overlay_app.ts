// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './initial_toast.js';
import './selection_overlay.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from '//resources/js/assert.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BigString} from '//resources/mojo/mojo/public/mojom/base/big_string.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import type {InitialToastElement} from './initial_toast.js';
import {getTemplate} from './lens_overlay_app.html.js';

export interface LensOverlayAppElement {
  $: {
    backgroundScrim: HTMLElement,
    closeButton: CrIconButtonElement,
    feedbackButton: CrIconButtonElement,
    initialToast: InitialToastElement,
  };
}

export class LensOverlayAppElement extends PolymerElement {
  static get is() {
    return 'lens-overlay-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      screenshotDataUri: String,
      closeButtonHidden: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isImageRendered: Boolean,
    };
  }

  // The data URI of the screenshot passed from C++.
  private screenshotDataUri: string;
  // Whether the close button should be hidden.
  private closeButtonHidden: boolean = false;
  // Whether the image has finished rendering.
  private isImageRendered: boolean = false;

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy.callbackRouter;
    this.listenerIds = [
      callbackRouter.screenshotDataUriReceived.addListener(
          this.screenshotDataUriReceived.bind(this)),
      callbackRouter.notifyResultsPanelOpened.addListener(
          this.onNotifyResultsPanelOpened.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  private onBackgroundScrimClicked() {
    this.browserProxy.handler.closeRequestedByOverlayBackgroundClick();
  }

  private onCloseButtonClick() {
    this.browserProxy.handler.closeRequestedByOverlayCloseButton();
  }

  private onFeedbackButtonClick() {
    this.browserProxy.handler.feedbackRequestedByOverlay();
  }

  private onNotifyResultsPanelOpened() {
    this.closeButtonHidden = true;
  }

  private screenshotDataUriReceived(dataUri: BigString) {
    const data: BigBuffer = dataUri.data;

    // TODO(b/334185985): This occurs when the browser failed to allocate the
    // memory for the string. Handle case when screenshot data URI encoding
    // fails.
    if (data.invalidBuffer) {
      return;
    }

    if (Array.isArray(data.bytes)) {
      this.screenshotDataUri =
          new TextDecoder().decode(new Uint8Array(data.bytes));
      return;
    }

    // If the buffer is not invalid or an array, it must be shared memory.
    assert(data.sharedMemory);
    const sharedMemory = data.sharedMemory;
    const {buffer, result} =
        sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
    assert(result === Mojo.RESULT_OK);
    this.screenshotDataUri = new TextDecoder().decode(buffer);
  }

  private closeInitialToast() {
    this.$.initialToast.triggerHideMessageAnimation();
  }

  private hideInitialToastGradient() {
    this.$.initialToast.triggerHideScrimAnimation();
  }

  private onScreenshotRendered() {
    this.isImageRendered = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-overlay-app': LensOverlayAppElement;
  }
}

customElements.define(LensOverlayAppElement.is, LensOverlayAppElement);

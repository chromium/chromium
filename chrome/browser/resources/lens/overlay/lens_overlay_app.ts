// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cursor_tooltip.js';
import './initial_toast.js';
import './selection_overlay.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BigString} from '//resources/mojo/mojo/public/mojom/base/big_string.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import type {CursorTooltipData, CursorTooltipElement} from './cursor_tooltip.js';
import type {InitialToastElement} from './initial_toast.js';
import {getTemplate} from './lens_overlay_app.html.js';

// Closes overlay if escape button is pressed.
function maybeCloseOverlay(event: KeyboardEvent) {
  if (event.key === 'Escape') {
    BrowserProxyImpl.getInstance()
        .handler.closeRequestedByOverlayEscapeKeyPress();
  }
}

export interface LensOverlayAppElement {
  $: {
    backgroundScrim: HTMLElement,
    closeButton: CrIconButtonElement,
    feedbackButton: CrIconButtonElement,
    infoButton: CrIconButtonElement,
    initialToast: InitialToastElement,
    cursorTooltip: CursorTooltipElement,
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
      isImageRendered: Boolean,
      closeButtonHidden: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isClosing: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  // The data URI of the screenshot passed from C++.
  private screenshotDataUri: string = '';
  // Whether the image has finished rendering.
  private isImageRendered: boolean = false;
  // Whether the close button should be hidden.
  private closeButtonHidden: boolean = false;
  // Whether the overlay is being shut down.
  private isClosing: boolean = false;


  private eventTracker_: EventTracker = new EventTracker();

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
      callbackRouter.notifyOverlayClosing.addListener(() => {
        this.isClosing = true;
      }),
    ];
    window.addEventListener('keyup', maybeCloseOverlay);
    this.eventTracker_.add(
        document, 'set-cursor-tooltip', (e: CustomEvent<CursorTooltipData>) => {
          this.$.cursorTooltip.setTooltip(e.detail.tooltipType);
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
    window.removeEventListener('keyup', maybeCloseOverlay);
    this.eventTracker_.removeAll();
  }

  override ready() {
    super.ready();
    this.addEventListener('pointermove', this.updateCursorPosition.bind(this));
  }

  private handlePointerEnter() {
    this.$.cursorTooltip.markPointerEnteredContentArea();
  }

  private handlePointerLeave() {
    this.$.cursorTooltip.markPointerLeftContentArea();
  }

  private handlePointerEnterActionButton() {
    this.$.cursorTooltip.hideTooltip();
  }

  private handlePointerLeaveActionButton() {
    this.$.cursorTooltip.showTooltip();
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

  private onInfoButtonClick(event: MouseEvent|KeyboardEvent) {
    this.browserProxy.handler.infoRequestedByOverlay({
      middleButton: (event as MouseEvent).button === 1,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
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

  private handleSelectionOverlayClicked() {
    this.$.initialToast.triggerHideMessageAnimation();
    this.$.cursorTooltip.setPauseTooltipChanges(true);
  }

  private handlePointerReleased() {
    this.$.initialToast.triggerHideScrimAnimation();
    this.$.cursorTooltip.setPauseTooltipChanges(false);
  }

  private onScreenshotRendered() {
    this.isImageRendered = true;
  }
  private getSelectionOverlayClass(screenshotDataUri: string): string {
    if (!screenshotDataUri || !screenshotDataUri.length) {
      return 'hidden';
    } else {
      return '';
    }
  }

  private updateCursorPosition(event: PointerEvent) {
    this.$.cursorTooltip.style.transform =
        `translate3d(${event.clientX}px, ${event.clientY}px, 0)`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-overlay-app': LensOverlayAppElement;
  }
}

customElements.define(LensOverlayAppElement.is, LensOverlayAppElement);

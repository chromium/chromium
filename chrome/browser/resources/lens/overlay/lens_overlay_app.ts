// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cursor_tooltip.js';
import './initial_toast.js';
import './selection_overlay.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from '//resources/js/assert.js';
import {skColorToHexColor} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BigString} from '//resources/mojo/mojo/public/mojom/base/big_string.mojom-webui.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme} from './color_utils.js';
import type {CursorTooltipData, CursorTooltipElement} from './cursor_tooltip.js';
import type {InitialToastElement} from './initial_toast.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {getTemplate} from './lens_overlay_app.html.js';
import {recordLensOverlayInteraction, UserAction} from './metrics_utils.js';

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
    moreOptionsButton: CrIconButtonElement,
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
      moreOptionsMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
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
  // Whether more options menu should be shown.
  private moreOptionsMenuVisible: boolean = false;
  // The overlay theme.
  private theme: OverlayTheme;

  private eventTracker_: EventTracker = new EventTracker();

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy.callbackRouter;
    this.listenerIds = [
      callbackRouter.screenshotDataUriReceived.addListener(
          this.screenshotDataUriReceived.bind(this)),
      callbackRouter.themeReceived.addListener(this.themeReceived.bind(this)),
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
    this.$.cursorTooltip.unhideTooltip();
  }

  private onBackgroundScrimClicked() {
    this.browserProxy.handler.closeRequestedByOverlayBackgroundClick();
  }

  private onCloseButtonClick() {
    this.browserProxy.handler.closeRequestedByOverlayCloseButton();
  }

  private onFeedbackClick() {
    this.browserProxy.handler.feedbackRequestedByOverlay();
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(UserAction.SEND_FEEDBACK);
  }

  private onLearnMoreClick(event: MouseEvent|KeyboardEvent) {
    this.browserProxy.handler.infoRequestedByOverlay({
      middleButton: (event as MouseEvent).button === 1,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(UserAction.LEARN_MORE);
  }

  private onMoreOptionsButtonClick() {
    this.moreOptionsMenuVisible = !this.moreOptionsMenuVisible;
  }

  private onMyActivityClick(event: MouseEvent|KeyboardEvent) {
    this.browserProxy.handler.activityRequestedByOverlay({
      middleButton: (event as MouseEvent).button === 1,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(UserAction.MY_ACTIVITY);
  }

  private onNotifyResultsPanelOpened() {
    this.closeButtonHidden = true;
  }

  private themeReceived(theme: OverlayTheme) {
    this.theme = theme;
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

  private skColorToHex_(skColor: SkColor): string {
    return skColorToHexColor(skColor);
  }

  private skColorToRgb_(skColor: SkColor): string {
    const hex = skColorToHexColor(skColor);
    assert(/^#[0-9a-fA-F]{6}$/.test(hex));
    const r = parseInt(hex.substring(1, 3), 16);
    const g = parseInt(hex.substring(3, 5), 16);
    const b = parseInt(hex.substring(5, 7), 16);
    return `${r}, ${g}, ${b}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-overlay-app': LensOverlayAppElement;
  }
}

customElements.define(LensOverlayAppElement.is, LensOverlayAppElement);

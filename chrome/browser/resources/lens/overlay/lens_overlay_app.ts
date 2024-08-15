// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cursor_tooltip.js';
import './initial_gradient.js';
import './selection_overlay.js';
import './translate_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from '//resources/js/assert.js';
import {skColorToHexColor} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getFallbackTheme} from './color_utils.js';
import type {CursorTooltipData, CursorTooltipElement} from './cursor_tooltip.js';
import {CursorTooltipType} from './cursor_tooltip.js';
import type {InitialGradientElement} from './initial_gradient.js';
import type {OverlayTheme} from './lens.mojom-webui.js';
import {UserAction} from './lens.mojom-webui.js';
import {getTemplate} from './lens_overlay_app.html.js';
import {recordLensOverlayInteraction, recordTimeToWebUIReady} from './metrics_utils.js';
import type {SelectionOverlayElement} from './selection_overlay.js';

export let INVOCATION_SOURCE: string = 'Unknown';

export interface LensOverlayAppElement {
  $: {
    backgroundScrim: HTMLElement,
    closeButton: CrIconButtonElement,
    copyToast: CrToastElement,
    cursorTooltip: CursorTooltipElement,
    initialGradient: InitialGradientElement,
    moreOptionsButton: CrIconButtonElement,
    moreOptionsMenu: HTMLElement,
    selectionOverlay: SelectionOverlayElement,
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
      initialFlashAnimationHasEnded: {
        type: Boolean,
        reflectToAttribute: true,
      },
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
      isTranslateButtonVisible: {
        type: Boolean,
        value: loadTimeData.getBoolean('enableOverlayTranslateButton'),
        readOnly: true,
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
  // Whether the initial flash animation has ended on the selection overlay.
  private initialFlashAnimationHasEnded: boolean = false;
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
  private invocationTime: number = loadTimeData.getValue('invocationTime');

  constructor() {
    super();

    this.browserProxy.handler.getOverlayInvocationSource().then(
        ({invocationSource}) => {
          INVOCATION_SOURCE = invocationSource;
        });
  }

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy.callbackRouter;
    this.listenerIds = [
      callbackRouter.screenshotDataReceived.addListener(
          this.screenshotDataReceived.bind(this)),
      callbackRouter.themeReceived.addListener(this.themeReceived.bind(this)),
      callbackRouter.notifyResultsPanelOpened.addListener(
          this.onNotifyResultsPanelOpened.bind(this)),
      callbackRouter.notifyOverlayClosing.addListener(() => {
        this.isClosing = true;
      }),
    ];
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
    this.eventTracker_.removeAll();
  }

  override ready() {
    super.ready();
    this.addEventListener('pointermove', this.updateCursorPosition.bind(this));
    recordTimeToWebUIReady(Number(Date.now() - this.invocationTime));
  }

  private handlePointerEnter() {
    this.$.cursorTooltip.markPointerEnteredContentArea();
  }

  private handlePointerLeave() {
    this.$.cursorTooltip.markPointerLeftContentArea();
  }

  private handlePointerEnterBackgroundScrim() {
    this.$.cursorTooltip.setTooltip(CursorTooltipType.LIVE_PAGE);
    this.$.cursorTooltip.unhideTooltip();
  }

  private handlePointerLeaveBackgroundScrim() {
    this.$.cursorTooltip.hideTooltip();
  }

  private handlePointerEnterSelectionOverlay() {
    this.$.cursorTooltip.unhideTooltip();
  }

  private handlePointerLeaveSelectionOverlay() {
    this.$.cursorTooltip.hideTooltip();
  }

  private onBackgroundScrimClicked() {
    this.browserProxy.handler.closeRequestedByOverlayBackgroundClick();
  }

  private onCloseButtonClick() {
    this.browserProxy.handler.closeRequestedByOverlayCloseButton();
  }

  private onFeedbackClick(event: MouseEvent|KeyboardEvent) {
    if (event instanceof KeyboardEvent &&
        !(event.key === 'Enter' || event.key === ' ')) {
      return;
    }
    this.browserProxy.handler.feedbackRequestedByOverlay();
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kSendFeedback);
  }

  private onLearnMoreClick(event: MouseEvent|KeyboardEvent) {
    if (event instanceof KeyboardEvent &&
        !(event.key === 'Enter' || event.key === ' ')) {
      return;
    }
    this.browserProxy.handler.infoRequestedByOverlay({
      middleButton: (event as MouseEvent).button === 1,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kLearnMore);
  }

  private onMoreOptionsButtonClick() {
    this.moreOptionsMenuVisible = !this.moreOptionsMenuVisible;
  }

  private onMyActivityClick(event: MouseEvent|KeyboardEvent) {
    if (event instanceof KeyboardEvent &&
        !(event.key === 'Enter' || event.key === ' ')) {
      return;
    }
    this.browserProxy.handler.activityRequestedByOverlay({
      middleButton: (event as MouseEvent).button === 1,
      altKey: event.altKey,
      ctrlKey: event.ctrlKey,
      metaKey: event.metaKey,
      shiftKey: event.shiftKey,
    });
    this.moreOptionsMenuVisible = false;
    recordLensOverlayInteraction(INVOCATION_SOURCE, UserAction.kMyActivity);
  }

  private onNotifyResultsPanelOpened() {
    this.closeButtonHidden = true;
  }

  private themeReceived(theme: OverlayTheme) {
    this.theme = theme;
  }

  private async screenshotDataReceived(screenshotData:
                                           BitmapMappedFromTrustedProcess) {
    const data: BigBuffer = screenshotData.pixelData;

    // TODO(b/334185985): This occurs when the browser failed to allocate the
    // memory for the pixels. Handle this case.
    if (data.invalidBuffer) {
      return;
    }

    // Pull the pixel data into a Uint8ClampedArray.
    let pixelData: Uint8ClampedArray;
    if (Array.isArray(data.bytes)) {
      pixelData = new Uint8ClampedArray(data.bytes);
    } else {
      // If the buffer is not invalid or an array, it must be shared memory.
      assert(data.sharedMemory);
      const sharedMemory = data.sharedMemory;
      const {buffer, result} =
          sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
      assert(result === Mojo.RESULT_OK);
      pixelData = new Uint8ClampedArray(buffer);
    }

    const imageWidth = screenshotData.imageInfo.width;
    const imageHeight = screenshotData.imageInfo.height;

    // Put our screenshot into ImageData object so it can be rendered in a
    // Canvas.
    const imageData = new ImageData(pixelData, imageWidth, imageHeight);
    const imageBitmap = await createImageBitmap(imageData);

    // Create a canvas that we can use to render the screenshot.
    const canvas = document.createElement('canvas');
    canvas.width = imageWidth;
    canvas.height = imageHeight;
    const ctx = canvas.getContext('bitmaprenderer')!;

    // Put the screenshot in the ctx to render.
    ctx.transferFromImageBitmap(imageBitmap);

    // Generate a blob that we can use across the app.
    // TODO(b/352622136): Blob generation is slow. We should render directly in
    // a JS canvas before converting to blob.
    canvas.toBlob((blob) => {
      assert(blob);
      this.screenshotDataUri = URL.createObjectURL(blob!);
    }, 'image/jpeg', 90);
  }

  private handleSelectionOverlayClicked() {
    this.$.cursorTooltip.setPauseTooltipChanges(true);
  }

  private handlePointerReleased() {
    this.$.initialGradient.triggerHideScrimAnimation();
    this.$.cursorTooltip.setPauseTooltipChanges(false);
  }

  private onScreenshotRendered() {
    this.isImageRendered = true;
  }

  private onInitialFlashAnimationEnd() {
    this.initialFlashAnimationHasEnded = true;
    this.$.initialGradient.setScrimVisible();
  }

  private async showTextCopiedToast() {
    if (this.$.copyToast.open) {
      // If toast already open, wait after hiding so that animation is
      // smoother.
      await this.$.copyToast.hide();
      setTimeout(() => {
        this.$.copyToast.show();
      }, 100);
    } else {
      this.$.copyToast.show();
    }
  }

  private onHideToastClick() {
    this.$.copyToast.hide();
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

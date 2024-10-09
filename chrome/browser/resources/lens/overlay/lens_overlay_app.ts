// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cursor_tooltip.js';
import './initial_gradient.js';
import './selection_overlay.js';
import './translate_button.js';
import '/lens/shared/searchbox_shared_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_components/searchbox/searchbox.js';

import {HelpBubbleMixin} from '//resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {skColorToHexColor} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
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
import type {TranslateButtonElement} from './translate_button.js';


export let INVOCATION_SOURCE: string = 'Unknown';

export interface LensOverlayAppElement {
  $: {
    backgroundScrim: HTMLElement,
    closeButton: CrIconButtonElement,
    cursorTooltip: CursorTooltipElement,
    initialGradient: InitialGradientElement,
    moreOptionsButton: CrIconButtonElement,
    moreOptionsMenu: HTMLElement,
    selectionOverlay: SelectionOverlayElement,
    toast: CrToastElement,
    translateButton: TranslateButtonElement,
    translateButtonContainer: HTMLDivElement,
  };
}

const LensOverlayAppElementBase = HelpBubbleMixin(I18nMixin(PolymerElement));

export class LensOverlayAppElement extends LensOverlayAppElementBase {
  static get is() {
    return 'lens-overlay-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isImageRendered: {
        type: Boolean,
        reflectToAttribute: true,
      },
      initialFlashAnimationHasEnded: {
        type: Boolean,
        reflectToAttribute: true,
      },
      sidePanelOpened: {
        type: Boolean,
        reflectToAttribute: true,
      },
      searchBoxHidden: {
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
      isTranslateButtonEnabled: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableOverlayTranslateButton'),
        readOnly: true,
        reflectToAttribute: true,
      },
      shouldFadeOutButtons: {
        type: Boolean,
        computed: 'computeShouldFadeOutButtons(isTranslateModeActive, ' +
            'isPointerDown)',
        reflectToAttribute: true,
      },
      isLensOverlayContextualSearchboxVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
      theme: {
        type: Object,
        value: getFallbackTheme,
      },
      darkMode: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('darkMode'),
        reflectToAttribute: true,
      },
      isSearchboxFocused: {
        type: Boolean,
        reflectToAttribute: true,
      },
      toastMessage: String,
    };
  }

  // Whether the translate button is enabled.
  private isTranslateButtonEnabled: boolean;
  // Whether the image has finished rendering.
  private isImageRendered: boolean = false;
  // Whether the initial flash animation has ended on the selection overlay.
  private initialFlashAnimationHasEnded: boolean = false;
  // Whether the side panel has been opened.
  private sidePanelOpened: boolean = false;
  // Whether the search box should be hidden. Updated on overlay selection and
  // translate mode state change.
  private searchBoxHidden: boolean = false;
  // Whether the overlay is being shut down.
  private isClosing: boolean = false;
  // Whether more options menu should be shown.
  private moreOptionsMenuVisible: boolean = false;
  // Whether the translate mode on the lens overlay has been activated. Updated
  // in response to events dispatched from the translate button.
  private isTranslateModeActive: boolean = false;
  // Whether the user is pressing down on the selection overlay. Updated in
  // response to events dispatched from the selection overlay.
  private isPointerDown: boolean = false;
  // Whether the button containers should be faded out.
  private shouldFadeOutButtons: boolean = false;
  // The overlay theme.
  private theme: OverlayTheme;
  // Whether the contextual searchbox is visible to the user.
  private isLensOverlayContextualSearchboxVisible: boolean = false;
  private toastMessage: string = '';
  // Whether the user is current focused into the searchbox.
  private isSearchboxFocused: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];
  private invocationTime: number = loadTimeData.getValue('invocationTime');

  // The ID returned by requestAnimationFrame for the updateCursorPosition
  // function.
  private updateCursorPositionRequestId?: number;

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
      callbackRouter.themeReceived.addListener(this.themeReceived.bind(this)),
      callbackRouter.shouldShowContextualSearchBox.addListener(
          this.shouldShowContextualSearchBox.bind(this)),
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
    this.eventTracker_.add(
        document, 'translate-mode-state-changed', (e: CustomEvent) => {
          this.isTranslateModeActive = e.detail.translateModeEnabled;
          this.searchBoxHidden =
              this.isTranslateModeActive || this.sidePanelOpened;
        });
    this.eventTracker_.add(document, 'text-copied', () => {
      this.showToast(this.i18n('copyToastMessage'));
    });
    this.eventTracker_.add(document, 'copied-as-image', () => {
      this.showToast(this.i18n('copyAsImageToastMessage'));
    });
    this.eventTracker_.add(
        this.$.translateButtonContainer, 'transitionend', () => {
          this.registerHelpBubble(
              'kLensOverlayTranslateButtonElementId',
              this.$.translateButton.getTranslateEnableButton());
          this.browserProxy.handler.maybeShowTranslateFeaturePromo();
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

  private handleSearchboxFocused() {
    this.isSearchboxFocused = true;
  }

  private handleSearchboxBlurred() {
    this.isSearchboxFocused = false;
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
    this.sidePanelOpened = true;
  }

  private themeReceived(theme: OverlayTheme) {
    this.theme = theme;
  }

  private shouldShowContextualSearchBox(shouldShow: boolean) {
    this.isLensOverlayContextualSearchboxVisible =
        loadTimeData.getBoolean('enableOverlayContextualSearchbox') &&
        shouldShow;
  }

  // The user started making a selection on the selection overlay.
  private handleSelectionStarted() {
    this.$.cursorTooltip.setPauseTooltipChanges(true);
    this.isPointerDown = true;
    this.searchBoxHidden = true;
  }

  // The user finished making their selection on the selection overlay.
  private handleSelectionFinished() {
    this.$.initialGradient.triggerHideScrimAnimation();
    this.$.cursorTooltip.setPauseTooltipChanges(false);
    this.isPointerDown = false;
  }

  private onScreenshotRendered() {
    this.isImageRendered = true;
  }

  private onInitialFlashAnimationEnd() {
    this.initialFlashAnimationHasEnded = true;
    this.$.initialGradient.setScrimVisible();
  }

  private computeShouldFadeOutButtons(): boolean {
    return !this.isTranslateModeActive && this.isPointerDown;
  }

  private async showToast(message: string) {
    if (this.$.toast.open) {
      // If toast already open, wait after hiding so that animation is
      // smoother.
      await this.$.toast.hide();
      setTimeout(() => {
        this.toastMessage = message;
        this.$.toast.show();
      }, 100);
    } else {
      this.toastMessage = message;
      this.$.toast.show();
    }
  }

  private onHideToastClick() {
    this.$.toast.hide();
  }



  private updateCursorPosition(event: PointerEvent) {
    // Cancel the previous animation frame to prevent the code from running more
    // than once a frame.
    if (this.updateCursorPositionRequestId) {
      cancelAnimationFrame(this.updateCursorPositionRequestId);
    }

    // Exit early if the tooltip is not visible.
    if (!this.$.cursorTooltip.isTooltipVisible()) {
      return;
    }

    this.updateCursorPositionRequestId = requestAnimationFrame(() => {
      this.$.cursorTooltip.style.transform =
          `translate3d(${event.clientX}px, ${event.clientY}px, 0)`;
      this.updateCursorPositionRequestId = undefined;
    });
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

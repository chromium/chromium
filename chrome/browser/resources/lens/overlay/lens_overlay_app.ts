// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cursor_tooltip.js';
import './initial_gradient.js';
import './selection_overlay.js';
import './translate_button.js';
import '/lens/shared/searchbox_ghost_loader.js';
import '/lens/shared/searchbox_shared_style.css.js';
import '//resources/cr_components/searchbox/searchbox.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/icons.html.js';

import {HelpBubbleMixin} from '//resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {skColorToHexColor} from '//resources/js/color_utils.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SearchboxGhostLoaderElement} from '/lens/shared/searchbox_ghost_loader.js';

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
import {PageContentType} from './page_content_type.mojom-webui.js';
import {PerformanceTracker} from './performance_tracker.js';
import {handleEscapeSearchbox} from './searchbox_utils.js';
import type {SelectionOverlayElement} from './selection_overlay.js';
import {focusShimmerOnRegion, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
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
    privacyNotice: HTMLElement,
    searchbox: SearchboxElement,
    searchboxContainer: HTMLElement,
    searchboxGhostLoader: SearchboxGhostLoaderElement,
    selectionOverlay: SelectionOverlayElement,
    toast: CrToastElement,
    translateButton: TranslateButtonElement,
    translateButtonContainer: HTMLElement,
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
      autocompleteRequestStarted: {
        type: Boolean,
        value: false,
      },
      enableBorderGlow: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableBorderGlow'),
      },
      enableCsbMotionTweaks: {
        reflectToAttribute: true,
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableCsbMotionTweaks'),
      },
      forceHideSearchBox: {
        type: Boolean,
        value: false,
      },
      isImageRendered: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      initialFlashAnimationHasEnded: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      sidePanelOpened: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      searchBoxHidden: {
        type: Boolean,
        computed:
            'computeShouldHideSearchBox(isTranslateModeActive, sidePanelOpened, forceHideSearchBox)',
        reflectToAttribute: true,
      },
      isClosing: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      moreOptionsMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
      isPointerDown: {
        type: Boolean,
        value: false,
      },
      isTranslateButtonEnabled: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableOverlayTranslateButton'),
        readOnly: true,
        reflectToAttribute: true,
      },
      isTranslateModeActive: {
        type: Boolean,
        value: false,
      },
      shouldFadeOutButtons: {
        type: Boolean,
        computed: 'computeShouldFadeOutButtons(isTranslateModeActive, ' +
            'isPointerDown)',
        reflectToAttribute: true,
      },
      isLensOverlayContextualSearchboxEnabled: {
        type: Boolean,
        reflectToAttribute: true,
        readOnly: true,
        value: () =>
            loadTimeData.getBoolean('enableOverlayContextualSearchbox'),
      },
      isLensOverlayContextualSearchboxVisible: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
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
        value: false,
      },
      suppressGhostLoader: {
        type: Boolean,
        value: false,
      },
      enableGhostLoader: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableGhostLoader'),
      },
      showGhostLoader: {
        type: Boolean,
        computed: `computeShowGhostLoader(
                enableGhostLoader,
                isSearchboxFocused,
                autocompleteRequestStarted,
                showErrorState,
                suppressGhostLoader)`,
        reflectToAttribute: true,
      },
      pageContentType: {
        type: Number,
        value: PageContentType.kUnknown,
      },
      placeholderText: {
        type: String,
        computed: `computePlaceholderText(pageContentType)`,
      },
      showErrorState: {
        type: Boolean,
        value: false,
        notify: true,
      },
      areLanguagePickersOpen: {
        type: Boolean,
        value: false,
      },
      toastMessage: {
        type: String,
        value: '',
      },
      enableCloseButtonTweaks: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableCloseButtonTweaks'),
        reflectToAttribute: true,
      },
      enableVisualSelectionUpdates: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableVisualSelectionUpdates'),
        reflectToAttribute: true,
      },
      searchboxSuggestionCount: {
        type: Number,
        value: 0,
      },
      canAnimateInCloseButton: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      overlayReshowInProgress: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      isPrivacyNoticeVisible: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('enablePrivacyNotice'),
      },
      hasPermissionsForSession: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => !loadTimeData.getBoolean('enablePrivacyNotice'),
      },
    };
  }

  // Whether the border glow is enabled via feature flag.
  declare enableBorderGlow: boolean;
  // Whether the user is currently focused into the searchbox.
  // Whether CSB motion tweaks are enabled via feature flag.
  declare enableCsbMotionTweaks: boolean;
  declare isSearchboxFocused: boolean;
  // Whether to purposely suppress the ghost loader. Done when escaping from
  // the searchbox when there's text (this doesn't create a zero suggset
  // request).
  declare suppressGhostLoader: boolean;
  // Whether the ghost loader should show its error state.
  declare showErrorState: boolean;
  // Whether this is an in flight request to autocomplete.
  declare private autocompleteRequestStarted: boolean;
  // Whether the translate button is enabled.
  declare private isTranslateButtonEnabled: boolean;
  // Whether the image has finished rendering.
  declare private isImageRendered: boolean;
  // Whether the initial flash animation has ended on the selection overlay.
  declare private initialFlashAnimationHasEnded: boolean;
  // Whether the side panel has been opened.
  declare private sidePanelOpened: boolean;
  // Whether the search box should be hidden.
  declare private searchBoxHidden: boolean;
  // Whether the search box should be forced to hide. Used to prevent the search
  // box from showing when we know the side panel will be opened.
  declare private forceHideSearchBox: boolean;
  // Whether the overlay is being shut down.
  declare private isClosing: boolean;
  // Whether more options menu should be shown.
  declare private moreOptionsMenuVisible: boolean;
  // Whether the translate mode on the lens overlay has been activated. Updated
  // in response to events dispatched from the translate button.
  declare private isTranslateModeActive: boolean;
  // Whether the user is pressing down on the selection overlay. Updated in
  // response to events dispatched from the selection overlay.
  declare private isPointerDown: boolean;
  // Whether the button containers should be faded out.
  declare private shouldFadeOutButtons: boolean;
  declare private darkMode: boolean;
  // The overlay theme.
  declare private theme: OverlayTheme;
  // Whether the contextual searchbox feature is enabled.
  declare private isLensOverlayContextualSearchboxEnabled: boolean;
  // Whether the contextual searchbox is visible to the user.
  declare private isLensOverlayContextualSearchboxVisible: boolean;
  // Whether the contextual searchbox should be auto-focused when the overlay is
  // first opened.
  private autoFocusSearchbox: boolean =
      loadTimeData.getValue('autoFocusSearchbox') &&
      !loadTimeData.getValue('enablePrivacyNotice');
  declare private toastMessage: string;
  declare private enableCloseButtonTweaks: boolean;
  declare private enableVisualSelectionUpdates: boolean;
  // The number of suggestions currently being shown to the user.
  declare private searchboxSuggestionCount: number;
  // Whether the close button can animate in. This is used in the new CSB
  // animation to ensure the close button animates in with the searchbox. Cannot
  // rely solely on isLensOverlayContextualSearchboxVisible because that might
  // never become true, which would prevent the close button from animating in.
  declare private canAnimateInCloseButton: boolean;
  // What the current page content type is.
  declare private pageContentType: PageContentType;
  // Whether the ghost loader is enabled via feature flag.
  declare private enableGhostLoader: boolean;
  // Whether to show the ghost loader.
  declare private showGhostLoader: boolean;
  // What the placeholder text should be.
  declare private placeholderText: string;
  // Whether the translate language pickers are open.
  declare private areLanguagePickersOpen: boolean;
  // Whether the overlay is currently being reshown.
  declare private overlayReshowInProgress: boolean;
  // Whether to show the privacy notice.
  declare private isPrivacyNoticeVisible: boolean;
  // Whether permissions have been granted for this session, either temporarily
  // following a user interaction, or permanently following the user accepting
  // the privacy notice.
  declare private hasPermissionsForSession: boolean;

  // The performance tracker used to log performance metrics for the overlay.
  private performanceTracker: PerformanceTracker = new PerformanceTracker();

  // Whether the overlay has received notice that the handshake with the Lens
  // backend has completed. The handshake is required to send suggest requests.
  private isBackendHandshakeComplete = false;
  // Whether to trigger the autocomplete request when suggest inputs are ready.
  private triggerSuggestOnInputReady =
      loadTimeData.getBoolean('enablePrivacyNotice');

  private eventTracker_: EventTracker = new EventTracker();

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];
  private invocationTime: number = loadTimeData.getValue('invocationTime');

  private searchboxBoundingClientRectObserver: ResizeObserver =
      new ResizeObserver(this.onSearchboxBoundsChanged.bind(this));

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
      callbackRouter.notifyHandshakeComplete.addListener(
          this.onBackendHandshakeComplete.bind(this)),
      callbackRouter.notifyResultsPanelOpened.addListener(
          this.onNotifyResultsPanelOpened.bind(this)),
      callbackRouter.notifyOverlayClosing.addListener(() => {
        this.isClosing = true;
        this.performanceTracker.endSession();
      }),
      callbackRouter.onOverlayReshown.addListener(() => {
        this.isClosing = false;
        this.sidePanelOpened = true;
        this.overlayReshowInProgress = true;
        this.hasPermissionsForSession = true;
        this.performanceTracker.reset();
        this.performanceTracker.startSession();
      }),
      callbackRouter.suppressGhostLoader.addListener(
          this.suppressGhostLoader_.bind(this)),
      callbackRouter.pageContentTypeChanged.addListener(
          this.onPageContentTypeChanged.bind(this)),
    ];
    this.eventTracker_.add(
        document, 'set-cursor-tooltip', (e: CustomEvent<CursorTooltipData>) => {
          this.$.cursorTooltip.setTooltip(e.detail.tooltipType);
        });
    this.eventTracker_.add(
        document, 'translate-mode-state-changed', (e: CustomEvent) => {
          this.isTranslateModeActive = e.detail.translateModeEnabled;
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
          this.eventTracker_.remove(
              this.$.translateButtonContainer, 'transitionend');
        });
    this.eventTracker_.add(document, 'language-picker-closed', () => {
      this.handleLanguagePickerClosed();
    });
    this.eventTracker_.add(document, 'language-picker-opened', () => {
      this.handleLanguagePickersOpened();
    });
    this.eventTracker_.add(
        document, 'query-autocomplete',
        this.handleQueryAutocomplete.bind(this));
    this.eventTracker_.add(
        document, 'pointermove', this.updateCursorPosition.bind(this));
    this.eventTracker_.add(this.$.searchbox, 'mousedown', () => {
      this.suppressGhostLoader = false;
    });
    this.eventTracker_.add(
        this.$.selectionOverlay, 'on-finish-reshow-overlay', () => {
          if (!this.overlayReshowInProgress) {
            return;
          }

          this.overlayReshowInProgress = false;
          this.browserProxy.handler.finishReshowOverlay();
        });

    this.performanceTracker.startSession();
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
    this.isPrivacyNoticeVisible = false;
    this.suppressGhostLoader = false;
    this.isSearchboxFocused = true;
    this.$.translateButtonContainer.classList.remove('searchbox-unfocused');

    this.focusShimmerOnSearchbox();

    // Setup a listener on the suggestions container to change the shimmer when
    // the searchbox changes sizes or the selection overlay changes size.
    this.searchboxBoundingClientRectObserver.observe(
        this.$.searchbox.getSuggestionsElement());
    this.searchboxBoundingClientRectObserver.observe(this.$.selectionOverlay);
  }

  // Called when the searchbox requests autocomplete suggestions.
  private handleQueryAutocomplete(e: CustomEvent) {
    // A request is only started for zero suggest, which is when the input value
    // is empty.
    this.autocompleteRequestStarted = !e.detail.inputValue;

    if (this.autocompleteRequestStarted && !window.navigator.onLine) {
      // If the user doesn't have an internet connection, the suggest request
      // will fail, so immediately show the error state.
      this.showErrorState = true;
      return;
    }

    this.showErrorState = false;
  }

  private onSearchboxBoundsChanged() {
    this.focusShimmerOnSearchbox();

    this.searchboxSuggestionCount =
        this.$.searchbox.getSuggestionsElement().selectableMatchElements.length;
  }

  private focusShimmerOnSearchbox() {
    const suggestionsContainer = this.$.searchbox.getSuggestionsElement();
    const areSuggestionsShowing =
        suggestionsContainer.offsetWidth * suggestionsContainer.offsetHeight >
        0;
    // If no suggestions are showing, default to the ghost loader size. If ghost
    // loader is not showing, default to the searchbox bounds.
    const newSearchboxWidth = areSuggestionsShowing ?
        suggestionsContainer.offsetWidth :
        (this.$.searchboxGhostLoader.offsetWidth > 0 ?
             this.$.searchboxGhostLoader.offsetWidth :
             this.$.searchbox.offsetWidth);
    const newSearchboxHeight = areSuggestionsShowing ?
        suggestionsContainer.offsetHeight :
        (this.$.searchboxGhostLoader.offsetHeight > 0 ?
             this.$.searchboxGhostLoader.offsetHeight :
             this.$.searchbox.offsetHeight);

    // Get the top and left position of the searchbox relative to the selection
    // overlay.
    const selectionOverlayRect = this.$.selectionOverlay.getBoundingRect();
    const newTop =
        this.$.searchboxContainer.offsetTop - selectionOverlayRect.top;
    const newLeft =
        this.$.searchboxContainer.offsetLeft - selectionOverlayRect.left;

    focusShimmerOnRegion(
        this, newTop / selectionOverlayRect.height,
        newLeft / selectionOverlayRect.width,
        newSearchboxWidth / selectionOverlayRect.width,
        newSearchboxHeight / selectionOverlayRect.height,
        ShimmerControlRequester.SEARCHBOX);
  }

  private handleSearchboxBlurred(event: FocusEvent) {
    // Ignore the blurred event if focus left one child element to enter another
    // child element.
    if (event.relatedTarget instanceof Node &&
        this.$.searchboxContainer.contains(event.relatedTarget)) {
      // TODO(380467089): This workaround wouldn't be needed if the ghost loader
      // was part of the searchbox element. Remove this workaround once they are
      // combined.
      return;
    }
    this.isSearchboxFocused = false;
    this.autocompleteRequestStarted = false;
    this.showErrorState = false;
    this.$.translateButtonContainer.classList.add('searchbox-unfocused');

    // Unfocus the shimmer.
    unfocusShimmer(this, ShimmerControlRequester.SEARCHBOX);

    // Disconnect the ResizeObserver.
    this.searchboxBoundingClientRectObserver.disconnect();
  }

  private handleEscapeSearchbox(e: CustomEvent) {
    handleEscapeSearchbox(this, this.$.searchbox, e);
  }

  private handleLanguagePickersOpened() {
    this.areLanguagePickersOpen = true;
  }

  private handleLanguagePickerClosed() {
    this.areLanguagePickersOpen = false;
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

  private computeShowGhostLoader(): boolean {
    // Ghost loader is disabled by the feature flag or suppressed by the
    // LensOverlayController. It is also disabled if permissions have not yet
    // been granted.
    if (!this.enableGhostLoader || this.suppressGhostLoader ||
        !this.hasPermissionsForSession) {
      return false;
    }
    // Show the ghost loader if there is focus on the searchbox, and there is
    // autcomplete is loading or if autocomplete failed.
    return this.isSearchboxFocused &&
        (this.autocompleteRequestStarted || this.showErrorState);
  }

  private computePlaceholderText(): string {
    return this.pageContentType === PageContentType.kPdf ?
        this.i18n('searchBoxHintPdf') :
        this.i18n('searchBoxHintDefault');
  }

  private suppressGhostLoader_() {
    // If tab is foregrounded don't show ghost loader.
    this.suppressGhostLoader = true;
  }

  private onPageContentTypeChanged(newPageContentType: PageContentType) {
    this.pageContentType = newPageContentType;
  }

  private onMoreOptionsButtonClick() {
    if (this.isTranslateButtonEnabled) {
      // Try to close the translate feature promo if it is currently active.
      // No-op if it is not active.
      this.browserProxy.handler.maybeCloseTranslateFeaturePromo(
          /*featureEngaged=*/ false);
    }
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

  private onBackendHandshakeComplete() {
    if (this.isBackendHandshakeComplete) {
      // The handshake should only be completed once on invocation. Ignore
      // subsequent calls just in case.
      return;
    }
    this.isBackendHandshakeComplete = true;

    // Trigger autocomplete if the handshake completed while the user is waiting
    // for suggest results.
    if (this.triggerSuggestOnInputReady && this.isSearchboxFocused) {
      this.triggerSearchboxSuggestions();
    }
    this.triggerSuggestOnInputReady = false;
  }

  private onNotifyResultsPanelOpened() {
    this.sidePanelOpened = true;
  }

  private themeReceived(theme: OverlayTheme) {
    this.theme = theme;
  }

  private shouldShowContextualSearchBox(shouldShow: boolean) {
    this.isLensOverlayContextualSearchboxVisible =
        this.isLensOverlayContextualSearchboxEnabled && shouldShow;
    this.canAnimateInCloseButton = true;
  }

  // The user started making a selection on the selection overlay.
  private handleSelectionStarted() {
    this.$.cursorTooltip.setPauseTooltipChanges(true);
    this.isPointerDown = true;
    this.forceHideSearchBox = true;
  }

  // The user finished making their selection on the selection overlay.
  private handleSelectionFinished() {
    if (!this.enableBorderGlow) {
      this.$.initialGradient.triggerHideScrimAnimation();
    }
    this.$.cursorTooltip.setPauseTooltipChanges(false);
    this.isPointerDown = false;
  }

  private onScreenshotRendered(e: CustomEvent<{isSidePanelOpen: boolean}>) {
    this.isImageRendered = true;
    this.sidePanelOpened = e.detail.isSidePanelOpen;
    // Focus the searchbox simultaneously with the initial flash animation.
    if (this.enableCsbMotionTweaks && this.autoFocusSearchbox &&
        this.isLensOverlayContextualSearchboxVisible) {
      this.focusSearchbox();
    }
  }

  private onInitialFlashAnimationEnd() {
    this.initialFlashAnimationHasEnded = true;
    if (!this.enableBorderGlow) {
      this.$.initialGradient.setScrimVisible();
    }
    // The searchbox is not focusable until the animation has ended.
    // Only called here if not already called in onScreenshotRendered
    if (this.autoFocusSearchbox &&
        this.isLensOverlayContextualSearchboxVisible &&
        !this.enableCsbMotionTweaks) {
      this.focusSearchbox();
    }
    if (loadTimeData.getValue('enablePrivacyNotice')) {
      // Focus the privacy notice to ensure all its elements are read by screen
      // readers.
      this.$.privacyNotice.focus();
    }
  }

  private triggerSearchboxSuggestions() {
    // If the user doesn't have an internet connection, the suggest request will
    // fail, so immediately show the error state.
    if (!window.navigator.onLine) {
      this.showErrorState = true;
      return;
    }

    // If the backend handshake has completed, then it is safe to issue the
    // autocomplete query immediately.
    if (this.isBackendHandshakeComplete) {
      this.$.searchbox.queryAutocomplete();
      return;
    }

    // Since the backend handshake has not completed, set the
    // triggerSuggestOnInputReady flag so that the autocomplete query is
    // triggered when the handshake completes. Also set the autocomplete request
    // started flag to true so that the ghost loader is shown to hide the
    // handshake latency from the user.
    this.triggerSuggestOnInputReady = true;
    this.autocompleteRequestStarted = true;
  }

  private focusSearchbox() {
    this.shadowRoot!.querySelector<HTMLElement>('cr-searchbox')
        ?.shadowRoot!.querySelector<HTMLElement>('input')
        ?.focus();
    this.triggerSearchboxSuggestions();
  }

  private computeShouldFadeOutButtons(): boolean {
    return !this.isTranslateModeActive && this.isPointerDown;
  }

  private computeShouldHideSearchBox(): boolean {
    return this.isTranslateModeActive || this.sidePanelOpened ||
        this.forceHideSearchBox;
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

  private getSearchboxAriaDescription(): string {
    // Get the the text from the ghost loader to add to the searchbox aria
    // description.
    return this.showGhostLoader ? this.$.searchboxGhostLoader.getText() : '';
  }

  private onPrivacyNoticeCancel() {
    this.browserProxy.handler.dismissPrivacyNotice();
    this.isPrivacyNoticeVisible = false;
  }

  private onPrivacyNoticeContinue() {
    this.browserProxy.handler.acceptPrivacyNotice();
    this.isPrivacyNoticeVisible = false;
    this.hasPermissionsForSession = true;
    this.focusSearchbox();
  }

  setSearchboxFocusForTesting(isFocused: boolean) {
    this.isSearchboxFocused = isFocused;
  }

  handleEscapeSearchboxForTesting(e: CustomEvent) {
    this.handleEscapeSearchbox(e);
  }

  getSidePanelOpenedForTesting(): boolean {
    return this.sidePanelOpened;
  }

  getOverlayReshowInProgressForTesting(): boolean {
    return this.overlayReshowInProgress;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-overlay-app': LensOverlayAppElement;
  }
}

customElements.define(LensOverlayAppElement.is, LensOverlayAppElement);

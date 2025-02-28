// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './side_panel_ghost_loader.js';
import '/strings.m.js';
import '/lens/shared/searchbox_ghost_loader.js';
import '/lens/shared/searchbox_shared_style.css.js';
import '//resources/cr_components/searchbox/searchbox.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixin} from '//resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SearchboxGhostLoaderElement} from '/lens/shared/searchbox_ghost_loader.js';

import type {LensSidePanelPageHandlerInterface} from '../lens_side_panel.mojom-webui.js';
import {PageContentType} from '../page_content_type.mojom-webui.js';
import {handleEscapeSearchbox, onSearchboxKeydown} from '../searchbox_utils.js';

import {getTemplate} from './side_panel_app.html.js';
import {SidePanelBrowserProxyImpl} from './side_panel_browser_proxy.js';
import type {SidePanelBrowserProxy} from './side_panel_browser_proxy.js';
import type {SidePanelGhostLoaderElement} from './side_panel_ghost_loader.js';

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

export interface LensSidePanelAppElement {
  $: {
    results: HTMLIFrameElement,
    ghostLoader: SidePanelGhostLoaderElement,
    networkErrorPage: HTMLElement,
    searchbox: SearchboxElement,
    searchboxContainer: HTMLElement,
    searchboxGhostLoader: SearchboxGhostLoaderElement,
    toast: CrToastElement,
    uploadProgressBar: HTMLElement,
    uploadProgressBarContainer: HTMLElement,
  };
}

const LensSidePanelAppElementBase = HelpBubbleMixin(I18nMixin(PolymerElement));
export class LensSidePanelAppElement extends LensSidePanelAppElementBase {
  static get is() {
    return 'lens-side-panel-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isBackArrowVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      isErrorPageVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      /* Used to decide whether to show back arrow onFocusOut in searchbox. */
      wasBackArrowAvailable: {
        type: Boolean,
        value: false,
      },
      isLoadingResults: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },
      isContextualSearchbox: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      loadingImageUrl: {
        type: String,
        value: loadTimeData.getString('resultsLoadingUrl'),
        readOnly: true,
      },
      darkMode: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('darkMode'),
        reflectToAttribute: true,
      },
      isSearchboxFocused: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      suppressGhostLoader: {
        type: Boolean,
        value: true,
      },
      showGhostLoader: {
        type: Boolean,
        computed: `computeShowGhostLoader(
                isSearchboxFocused,
                autocompleteRequestStarted,
                showErrorState,
                suppressGhostLoader,
                isContextualSearchbox)`,
        reflectToAttribute: true,
      },
      showErrorState: {
        type: Boolean,
        value: false,
        notify: true,
      },
      /* TODO(385183449): Once WebUI preloading is implemented in the
       * side panel, update the loadTimeData for searchBoxHint in the side
       * panel WebUI constructor insteading of passing it to the searchbox. */
      placeholderText: {
        type: String,
        computed:
            `computePlaceholderText(isContextualSearchbox, pageContentType)`,
      },
      uploadProgressPercentage: {
        type: Number,
        value: 0,
      },
      showUploadProgress: {
        type: Number,
        computed:
            `computeShowUploadProgress(uploadProgressPercentage)`,
        reflectToAttribute: true,
      },
      toastMessage: String,
    };
  }

  // Public for use in browser tests.
  isBackArrowVisible: boolean;
  // Whether the user is currently focused into the searchbox.
  isSearchboxFocused: boolean;
  // Whether to purposely suppress the ghost loader. Done when escaping from
  // the searchbox when there's text or when page bytes aren't successfully
  // uploaded.
  suppressGhostLoader: boolean;
  // Whether the ghost loader should show its error state.
  showErrorState: boolean;
  // The current progress of the page content upload.
  uploadProgressPercentage: number;
  // The placeholder text to show in the searchbox.
  private pageContentType: PageContentType = PageContentType.kUnknown;
  // Whether this is an in flight request to autocomplete.
  private autocompleteRequestStarted: boolean = false;
  private isErrorPageVisible: boolean;
  // Whether the results iframe is currently loading. This needs to be done via
  // browser because the iframe is cross-origin. Default true since the side
  // panel can open before a navigation has started.
  private isLoadingResults: boolean;
  private isContextualSearchbox: boolean;
  // The URL for the loading image shown when results frame is loading a new
  // page.
  private readonly loadingImageUrl: string;
  // The animations for the progress bar. One for the progress bar width
  // increase, and one for the progress bar height decrease on results load.
  private progressBarAnimation: Animation|null = null;
  private progressBarHideAnimation: Animation|null = null;

  private browserProxy: SidePanelBrowserProxy =
      SidePanelBrowserProxyImpl.getInstance();
  private darkMode: boolean;
  private listenerIds: number[];
  private pageHandler: LensSidePanelPageHandlerInterface;
  private wasBackArrowAvailable: boolean;
  private toastMessage: string = '';
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    // Need to get the page classification through a mojom call since the
    // WebUI controller doesn't have access to the lens overlay controller
    // in time to set this as a loadTimeData.
    this.browserProxy.handler.getIsContextualSearchbox().then(
        ({isContextualSearchbox}) => {
          this.isContextualSearchbox = isContextualSearchbox;
        });
    this.pageHandler = SidePanelBrowserProxyImpl.getInstance().handler;
    ColorChangeUpdater.forDocument().start();
  }

  override ready() {
    super.ready();

    this.registerHelpBubble(
        'kLensSidePanelSearchBoxElementId', this.$.searchbox);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds = [
      this.browserProxy.callbackRouter.loadResultsInFrame.addListener(
          this.loadResultsInFrame.bind(this)),
      this.browserProxy.callbackRouter.setIsLoadingResults.addListener(
          this.setIsLoadingResults.bind(this)),
      this.browserProxy.callbackRouter.setPageContentUploadProgress.addListener(
          this.setPageContentUploadProgress.bind(this)),
      this.browserProxy.callbackRouter.setBackArrowVisible.addListener(
          this.setBackArrowVisible.bind(this)),
      this.browserProxy.callbackRouter.setShowErrorPage.addListener(
          this.setShowErrorPage.bind(this)),
      this.browserProxy.callbackRouter.suppressGhostLoader.addListener(
          this.suppressGhostLoader_.bind(this)),
      this.browserProxy.callbackRouter.pageContentTypeChanged.addListener(
          this.pageContentTypeChanged.bind(this)),
      this.browserProxy.callbackRouter.showToast.addListener(
          this.showToast.bind(this)),
    ];
    this.eventTracker_.add(this.$.searchbox, 'mousedown', () => {
      this.suppressGhostLoader = false;
      this.showErrorState = false;
    });
    this.eventTracker_.add(document, 'keydown', (event: KeyboardEvent) => {
      if (event.key !== 'Escape' && this.isSearchboxFocused) {
        onSearchboxKeydown(this, this.$.searchbox);
      }
    });
    this.eventTracker_.add(
        document, 'query-autocomplete',
        this.handleQueryAutocomplete.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
    this.eventTracker_.removeAll();
  }

  private onBackArrowClick() {
    this.pageHandler.popAndLoadQueryFromHistory();
  }

  private setIsLoadingResults(isLoading: boolean) {
    if (this.isLoadingResults === isLoading) {
      return;
    }
    this.isLoadingResults = isLoading;

    if (this.isLoadingResults) {
      // The user submitted a new query, therefore the searchbox should not stay
      // focused.
      this.blurSearchbox();
    } else {
      // Animate away the progress bar once the results are loaded.
      this.progressBarHideAnimation = this.$.uploadProgressBarContainer.animate(
          {height: ['0px'], transform: ['scaleY(1)', 'scaleY(0)']}, {
            duration: 200,
            easing: 'cubic-bezier(0.3, 0, 0.8, 0.15)',
            fill: 'forwards',
          });
      this.progressBarHideAnimation.onfinish = () => {
        // Reset progress bar to 0 so the next upload starts from the beginning
        // and the progress bar stays invisible.
        this.uploadProgressPercentage = 0;
      };
    }
  }

  private setPageContentUploadProgress(progress: number) {
    // If the results are not loading, then the progress bar should not be
    // shown.
    if (!this.isLoadingResults) {
      return;
    }

    if (this.uploadProgressPercentage === 0) {
      // Restart the progress bar animations to ensure the progress bar is
      // visible and animates from the beginning.
      this.progressBarAnimation?.cancel();
      this.progressBarHideAnimation?.cancel();
    }

    this.uploadProgressPercentage = progress * 100;

    // Control the progress bar animation.
    this.progressBarAnimation = this.$.uploadProgressBar.animate(
        {
          width: [this.uploadProgressPercentage + '%'],
        },
        {
          duration: this.uploadProgressPercentage === 100 ? 400 : 1000,
          easing: 'cubic-bezier(0.2, 0.0, 0, 1.0)',
          fill: 'forwards',
        });
  }

  private loadResultsInFrame(resultsUrl: Url) {
    const url = new URL(resultsUrl.url);
    const resultsBoundingRect = this.$.results.getBoundingClientRect();
    if (resultsBoundingRect.width > 0) {
      url.searchParams.set(
          VIEWPORT_WIDTH_KEY, resultsBoundingRect.width.toString());
    }
    if (resultsBoundingRect.height > 0) {
      url.searchParams.set(
          VIEWPORT_HEIGHT_KEY, resultsBoundingRect.height.toString());
    }
    // The src needs to be reset explicitly every time this function is called
    // to force a reload. We cannot get the currently displayed URL from the
    // frame because of cross-origin restrictions.
    this.$.results.src = url.href;
    // Remove focus from the input when results are loaded. Does not have
    // any effect if input is not focused.
    this.blurSearchbox();
  }

  private blurSearchbox() {
    this.shadowRoot!.querySelector<HTMLElement>('cr-searchbox')
        ?.shadowRoot!.querySelector<HTMLElement>('input')
        ?.blur();
  }

  private handleEscapeSearchbox(e: CustomEvent) {
    handleEscapeSearchbox(this, this.$.searchbox, e);
  }

  private setBackArrowVisible(visible: boolean) {
    this.isBackArrowVisible = visible;
    this.wasBackArrowAvailable = visible;
  }

  // Called when the searchbox requests autocomplete suggestions.
  private handleQueryAutocomplete() {
    this.autocompleteRequestStarted = true;
  }

  private setShowErrorPage(shouldShowErrorPage: boolean) {
    this.isErrorPageVisible =
        shouldShowErrorPage && loadTimeData.getBoolean('enableErrorPage');
  }

  private onSearchboxFocusIn_() {
    this.isBackArrowVisible = false;
    this.suppressGhostLoader = false;
    this.isSearchboxFocused = true;
    this.notifyHelpBubbleAnchorCustomEvent(
        'kLensSidePanelSearchBoxElementId',
        'kLensSidePanelSearchBoxFocusedEventId');
  }

  private onSearchboxFocusOut_(event: FocusEvent) {
    // Ignore the blurred event if focus left one child element to enter another
    // child element.
    if (event.relatedTarget instanceof Node &&
        this.$.searchboxContainer.contains(event.relatedTarget)) {
      // TODO(380467089): This workaround wouldn't be needed if the ghost loader
      // was part of the searchbox element. Remove this workaround once they are
      // combined.
      return;
    }
    this.isBackArrowVisible = this.wasBackArrowAvailable;
    this.isSearchboxFocused = false;
    this.autocompleteRequestStarted = false;
    this.showErrorState = false;
  }

  private computeShowGhostLoader(): boolean {
    if (!this.isContextualSearchbox || this.suppressGhostLoader) {
      return false;
    }
    // Show the ghost loader if there is focus on the searchbox, and there is
    // autcomplete is loading or if autocomplete failed.
    return this.isSearchboxFocused &&
        (this.autocompleteRequestStarted || this.showErrorState);
  }

  private computePlaceholderText(): string {
    if (!this.isContextualSearchbox) {
      return '';
    }
    return this.pageContentType === PageContentType.kPdf ?
        this.i18n('searchBoxHintContextualPdf') :
        this.i18n('searchBoxHintContextualDefault');
  }

  private computeShowUploadProgress(): boolean {
    return this.uploadProgressPercentage > 0;
  }

  private getSearchboxAriaDescription(): string {
    // Get the the text from the ghost loader to add to the searchbox aria
    // description.
    return this.$.searchboxGhostLoader.getText();
  }

  private suppressGhostLoader_() {
    // If page bytes weren't successfully uploaded, ghost loader shouldn't be
    // visible.
    this.suppressGhostLoader = true;
  }

  private pageContentTypeChanged(newPageContentType: PageContentType) {
    this.pageContentType = newPageContentType;
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

  makeGhostLoaderVisibleForTesting() {
    this.isContextualSearchbox = true;
    this.suppressGhostLoader = false;
    this.isSearchboxFocused = true;
    this.autocompleteRequestStarted = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-side-panel-app': LensSidePanelAppElement;
  }
}

customElements.define(LensSidePanelAppElement.is, LensSidePanelAppElement);

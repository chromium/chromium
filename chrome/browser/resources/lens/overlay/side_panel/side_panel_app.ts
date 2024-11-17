// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '/lens/shared/searchbox_ghost_loader.js';
import '/lens/shared/searchbox_shared_style.css.js';
import '//resources/cr_components/searchbox/searchbox.js';
import './side_panel_ghost_loader.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixin} from '//resources/cr_components/help_bubble/help_bubble_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {LensSidePanelPageHandlerInterface} from '../lens.mojom-webui.js';

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
    networkErrorPage: HTMLDivElement,
  };
}

const LensSidePanelAppElementBase = HelpBubbleMixin(PolymerElement);
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
        computed: `computeShowGhostLoader(isSearchboxFocused,
              suppressGhostLoader, isContextualSearchbox)`,
        reflectToAttribute: true,
      },
      showErrorState: {
        type: Boolean,
        value: false,
        notify: true,
      },
    };
  }

  // Public for use in browser tests.
  isBackArrowVisible: boolean;
  private isErrorPageVisible: boolean;
  // Whether the results iframe is currently loading. This needs to be done via
  // browser because the iframe is cross-origin. Default true since the side
  // panel can open before a navigation has started.
  private isLoadingResults: boolean;
  private isSearchboxFocused: boolean;
  private isContextualSearchbox: boolean;
  private suppressGhostLoader: boolean;
  private showErrorState: boolean;
  // The URL for the loading image shown when results frame is loading a new
  // page.
  private readonly loadingImageUrl: string;

  private browserProxy: SidePanelBrowserProxy =
      SidePanelBrowserProxyImpl.getInstance();
  private darkMode: boolean;
  private listenerIds: number[];
  private pageHandler: LensSidePanelPageHandlerInterface;
  private wasBackArrowAvailable: boolean;

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

    const searchbox =
        this.shadowRoot!.querySelector<HTMLElement>('cr-searchbox');
    if (searchbox) {
      searchbox.addEventListener('focusin', () => this.onSearchboxFocusIn_());
      searchbox.addEventListener('focusout', () => this.onSearchboxFocusOut_());
      this.registerHelpBubble('kLensSidePanelSearchBoxElementId', searchbox);
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds = [
      this.browserProxy.callbackRouter.loadResultsInFrame.addListener(
          this.loadResultsInFrame.bind(this)),
      this.browserProxy.callbackRouter.setIsLoadingResults.addListener(
          this.setIsLoadingResults.bind(this)),
      this.browserProxy.callbackRouter.setBackArrowVisible.addListener(
          this.setBackArrowVisible.bind(this)),
      this.browserProxy.callbackRouter.setShowErrorPage.addListener(
          this.setShowErrorPage.bind(this)),
      this.browserProxy.callbackRouter.updateGhostLoaderState.addListener(
          this.updateGhostLoaderState.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  private onBackArrowClick() {
    this.pageHandler.popAndLoadQueryFromHistory();
  }

  private setIsLoadingResults(isLoading: boolean) {
    this.isLoadingResults = isLoading;
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
    this.shadowRoot!.querySelector<HTMLElement>('cr-searchbox')
        ?.shadowRoot!.querySelector<HTMLElement>('input')
        ?.blur();
  }

  private setBackArrowVisible(visible: boolean) {
    this.isBackArrowVisible = visible;
    this.wasBackArrowAvailable = visible;
  }

  private setShowErrorPage(shouldShowErrorPage: boolean) {
    this.isErrorPageVisible =
        shouldShowErrorPage && loadTimeData.getBoolean('enableErrorPage');
  }

  private onSearchboxFocusIn_() {
    this.isBackArrowVisible = false;
    this.isSearchboxFocused = true;
    this.notifyHelpBubbleAnchorCustomEvent(
        'kLensSidePanelSearchBoxElementId',
        'kLensSidePanelSearchBoxFocusedEventId');
  }

  private onSearchboxFocusOut_() {
    this.isBackArrowVisible = this.wasBackArrowAvailable;
    this.isSearchboxFocused = false;
  }

  private computeShowGhostLoader(): boolean {
    return this.isSearchboxFocused && !this.suppressGhostLoader &&
        this.isContextualSearchbox;
  }

  private updateGhostLoaderState(
      suppressGhostLoader: boolean, resetLoadingState: boolean) {
    // If page bytes weren't successfully uploaded, ghost loader shouldn't be
    // visible.
    this.suppressGhostLoader = suppressGhostLoader;
    if (resetLoadingState) {
      this.showErrorState = false;
    }
  }

  makeGhostLoaderVisibleForTesting() {
    this.isContextualSearchbox = true;
    this.suppressGhostLoader = false;
    this.isSearchboxFocused = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-side-panel-app': LensSidePanelAppElement;
  }
}

customElements.define(LensSidePanelAppElement.is, LensSidePanelAppElement);

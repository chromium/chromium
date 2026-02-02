// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import type {ClickInfo} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {handleBrowserCommand, handleModuleEvent, handlePageLoadMetric, handleScrollDepthMetric, handleTimeOnPageMetric} from './handlers.js';
import {EventType} from './types.js';
import type {BrowserCommand, DebugInfo, EventData} from './types.js';
import {getCss} from './whats_new_app.css.js';
import {getHtml} from './whats_new_app.html.js';
import {WhatsNewProxyImpl} from './whats_new_proxy.js';

declare const window: Window&{
  chromeWhatsNew: {
    debugInfo: () => DebugInfo,
    triggerBrowserCommand: (commandId: number) => void,
  },
};

// Parse URLSearchParams from a full url string.
function parseQueryParams(url: string): URLSearchParams {
  const parsedUrl = URL.parse(url);
  return new URLSearchParams(parsedUrl?.search);
}

// Parse the version query param. Returns 0 on failure.
function getRequestedVersion(params: URLSearchParams): number {
  const version = Number.parseInt(params.get('version') ?? '');
  return Number.isNaN(version) ? 0 : version;
}

// Parse a string array param. Returns empty array when missing.
function getArrayParam(params: URLSearchParams, key: string): string[] {
  const str = params.get(key) ?? '';
  return str.length ? str.split(',') : [];
}

export class WhatsNewAppElement extends CrLitElement {
  static get is() {
    return 'whats-new-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      url_: {type: String},
      isStaging_: {type: Boolean},
    };
  }

  protected accessor url_: string = '';
  protected accessor isStaging_: boolean = loadTimeData.getBoolean('isStaging');

  private beforeUnloadHandlerBound_:
      () => void = this.beforeUnloadHandler_.bind(this);
  private debugInfo_: DebugInfo = {error: 'whats_new_app not created'};
  private eventTracker_: EventTracker = new EventTracker();
  private heartbeatTimeOnPage_: number = 0;
  private isAutoOpen_: boolean = false;

  constructor() {
    super();

    // Set up window API.
    window.chromeWhatsNew = {
      debugInfo: () => this.debugInfo_,
      triggerBrowserCommand: (commandId: number) => {
        const data: BrowserCommand = {
          event: EventType.BROWSER_COMMAND,
          commandId,
          clickInfo: ({} as ClickInfo),
        };
        this.handleMessage_({
          origin: URL.parse(this.url_)?.origin,
          data: {data},
        } as MessageEvent);
      },
    };

    const queryParams = new URLSearchParams(window.location.search);

    // Indicates this tab was added automatically by the browser.
    this.isAutoOpen_ = queryParams.has('auto');

    // There are no subpages in What's New. Also remove the query param here
    // since its value is recorded.
    window.history.replaceState(undefined /* stateObject */, '', '/');

    this.debugInfo_ = {
      environment: this.isStaging_ ? 'staging' : 'production',
      autoOpened: this.isAutoOpen_,
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    WhatsNewProxyImpl.getInstance()
        .handler.getServerUrl(this.isStaging_)
        .then(({url}: {url: Url}) => this.handleUrlResult_(url));

    // Beforeunload events are unreliable in iframes when the tab is closed
    // directly. Using this event in the top-level frame is more robust in
    // ensuring this metric is recorded.
    window.addEventListener('beforeunload', this.beforeUnloadHandlerBound_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
    window.removeEventListener('beforeunload', this.beforeUnloadHandlerBound_);
  }

  private beforeUnloadHandler_() {
    if (this.heartbeatTimeOnPage_ > 0) {
      handleTimeOnPageMetric(this.heartbeatTimeOnPage_, true);
    }
  }

  /**
   * Handles the URL result of sending the initialize WebUI message.
   * @param url The What's New URL to use in the iframe.
   */
  private handleUrlResult_(url: string|null) {
    if (!url) {
      // This occurs in the special case of tests where we don't want to load
      // remote content.
      return;
    }

    const latest = this.isAutoOpen_ && !isChromeOS ? 'true' : 'false';
    url += url.includes('?') ? '&' : '?';
    // The browser has auto-opened the page due to an upgrade.
    // Let the embedded page know to display the "up to date" banner.
    this.url_ = url.concat(`updated=${latest}`);

    this.eventTracker_.add(
        window, 'message',
        (event: Event) => this.handleMessage_(event as MessageEvent));

    const parsedParams = parseQueryParams(this.url_);
    this.debugInfo_.requestedVersion = getRequestedVersion(parsedParams);
    this.debugInfo_.requestedEnabledFeatures =
        getArrayParam(parsedParams, 'enabled');
    this.debugInfo_.requestedRolledFeatures =
        getArrayParam(parsedParams, 'rolled');
  }

  private handleMessage_(event: MessageEvent) {
    if (!this.url_) {
      return;
    }

    const iframeUrl = new URL(this.url_);
    if (!event.data || event.origin !== iframeUrl.origin) {
      return;
    }

    const data = (event.data as EventData).data;
    if (!data) {
      return;
    }

    switch (data.event) {
      case EventType.BROWSER_COMMAND:
        handleBrowserCommand(data);
        break;
      case EventType.PAGE_LOADED:
        handlePageLoadMetric(data, this.isAutoOpen_);
        this.debugInfo_.renderedVersion = data.version ?? data.page_uid;
        break;
      case EventType.MODULES_RENDERED:
        this.debugInfo_.renderedModules = data.spotlight_modules;
        break;
      case EventType.EXPLORE_MORE_OPEN:
        WhatsNewProxyImpl.getInstance().handler.recordExploreMoreToggled(true);
        break;
      case EventType.EXPLORE_MORE_CLOSE:
        WhatsNewProxyImpl.getInstance().handler.recordExploreMoreToggled(false);
        break;
      case EventType.SCROLL:
        handleScrollDepthMetric(data);
        break;
      case EventType.TIME_ON_PAGE_MS:
        // If heartbeat metric has been recorded, ignore this event.
        if (this.heartbeatTimeOnPage_ === 0) {
          handleTimeOnPageMetric(data.time, false);
        }
        break;
      case EventType.MODULE_IMPRESSION:
      case EventType.GENERAL_LINK_CLICK:
      case EventType.VIDEO_STARTED:
      case EventType.VIDEO_ENDED:
      case EventType.PLAY_CLICKED:
      case EventType.PAUSE_CLICKED:
      case EventType.RESTART_CLICKED:
      case EventType.EXPEND_MEDIA:
      case EventType.CLOSE_EXPANDED_MEDIA:
        handleModuleEvent(data);
        break;
      case EventType.QR_CODE_TOGGLE_OPEN:
        WhatsNewProxyImpl.getInstance().handler.recordQrCodeToggled(true);
        break;
      case EventType.QR_CODE_TOGGLE_CLOSE:
        WhatsNewProxyImpl.getInstance().handler.recordQrCodeToggled(false);
        break;
      case EventType.NAV_CLICK:
        WhatsNewProxyImpl.getInstance().handler.recordNavClick();
        break;
      case EventType.FEATURE_TILE_NAVIGATION:
        WhatsNewProxyImpl.getInstance().handler.recordFeatureTileNavigation();
        break;
      case EventType.CAROUSEL_SCROLL_BUTTON_CLICK:
        WhatsNewProxyImpl.getInstance()
            .handler.recordCarouselScrollButtonClick();
        break;
      case EventType.CTA_CLICK:
        WhatsNewProxyImpl.getInstance().handler.recordCtaClick();
        break;
      case EventType.NEXT_BUTTON_CLICK:
        WhatsNewProxyImpl.getInstance().handler.recordNextButtonClick();
        break;
      case EventType.TIME_ON_PAGE_HEARTBEAT_MS:
        if (Number.isInteger(data.time)) {
          this.heartbeatTimeOnPage_ =
              Math.max(this.heartbeatTimeOnPage_, data.time);
        }
        break;
      default:
        console.warn('Unrecognized message.', data);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'whats-new-app': WhatsNewAppElement;
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);

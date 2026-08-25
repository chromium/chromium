// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';

import type {BrowserProxy} from './browser_proxy.js';
import {GuestPageType, WebClientState as WebClientStateMojo, ZoomAction} from './glic.mojom-webui.js';
import {DetailedWebClientState, GlicApiCommunicator, GlicApiHost, WebClientState} from './glic_api_impl/host/glic_api_host.js';
import {ObservableValue} from './observable.js';
import type {ObservableValueReadOnly} from './observable.js';
import {GlicRequestHeaderInjector} from './shared/glic_request_headers.js';
import type {WebViewType} from './shared/web_view_type.js';

// LINT.IfChange(GlicZoomFactors)
// Any changes to the range of supported zoom factors must be mirrored in
// GlicPageHandler.OnZoomLevelChange and guest_util.cc.
const ZOOM_FACTORS = [
  1.0,
  1.1,
  1.25,
  1.5,
  1.75,
  2.0,
];
// LINT.ThenChange(//chrome/browser/glic/host/glic_page_handler.cc:GlicZoomFactors,
// //chrome/browser/glic/host/guest_util.cc:GlicZoomFactors)

const ZOOM_DELTA_THRESHOLD = 0.01;

/**
 * Finds the next higher zoom factor from ZOOM_FACTORS relative to currentZoom.
 * Scans left-to-right to find the smallest factor strictly greater than
 * currentZoom. ZOOM_DELTA_THRESHOLD handles floating point representation
 * precision (e.g. 1.09 vs 1.1). Returns undefined if currentZoom is at or above
 * the maximum zoom factor.
 */
function findNextZoomInFactor(currentZoom: number): number|undefined {
  return ZOOM_FACTORS.find(f => f - currentZoom >= ZOOM_DELTA_THRESHOLD);
}

/**
 * Finds the next lower zoom factor from ZOOM_FACTORS relative to currentZoom.
 * Scans right-to-left to find the largest factor strictly smaller than
 * currentZoom. ZOOM_DELTA_THRESHOLD handles floating point representation
 * precision (e.g. 1.09 vs 1.1). Returns undefined if currentZoom is at or below
 * the minimum zoom factor.
 */
function findNextZoomOutFactor(currentZoom: number): number|undefined {
  return ZOOM_FACTORS.findLast(f => currentZoom - f >= ZOOM_DELTA_THRESHOLD);
}

interface ZoomChangeEventData {
  newZoomFactor: number;
}

function isZoomChangeEvent(e: Event): e is Event&ZoomChangeEventData {
  return 'newZoomFactor' in e && typeof e.newZoomFactor === 'number';
}

// Calls from the webview to its owner.
export interface WebviewDelegate {
  // Called when there is an error during page load.
  webviewError(reason: string): void;
  // Called when a page commits inside the webview.
  webviewPageCommit(pageType: GuestPageType, isApiAllowed: boolean): void;
  // Called when the webview redirects to an access error page.
  webviewDeniedByAdmin(): void;
}

// State for the WebviewController which lives as long as the WebUI content.
// This is necessary because we may destroy and rebuild the WebviewController
// multiple times.
export class WebviewPersistentState {
  // Normally, we load only the glicGuestURL. However, if that guest decides to
  // navigate to a different URL after the client connects, we will remember
  // that URL for loading later. To avoid getting stuck on a bad URL, we will
  // allow using `loadUrl` only once unless a client successfully connects.
  // Note that this supports internal development.
  private loadUrl: string|undefined;
  private loadUrlUsed = false;

  useLoadUrl(): string {
    if (this.loadUrl && !this.loadUrlUsed) {
      this.loadUrlUsed = true;
      return this.loadUrl;
    } else {
      return loadTimeData.getString('glicGuestURL');
    }
  }

  onCommitAfterConnect(newUrl: string) {
    this.loadUrl = newUrl;
    this.loadUrlUsed = false;
  }

  onClientReady() {
    // Web client became ready, allow loadUrl to be used again.
    this.loadUrlUsed = false;
  }
}

// Creates and manages the <webview> element, and the GlicApiHost which
// communicates with it.
export class WebviewController {
  webview: WebViewType;
  private host?: GlicApiHost;
  private dormant = false;
  private communicator?: GlicApiCommunicator;
  private onDestroy: Array<() => void> = [];
  private eventTracker = new EventTracker();
  private webClientState =
      ObservableValue.withValue(WebClientState.UNINITIALIZED);
  private glicRequestHeaderInjector?: GlicRequestHeaderInjector;
  private displayScaleMultiplier = 1.0;
  private webClientStateListenerId?: number;

  constructor(
      private readonly container: HTMLElement,
      private browserProxy: BrowserProxy,
      private delegate: WebviewDelegate,
      private persistentState: WebviewPersistentState,
  ) {
    this.webview = document.createElement('webview');

    this.glicRequestHeaderInjector = new GlicRequestHeaderInjector(
        this.webview, loadTimeData.getString('chromeVersion'),
        loadTimeData.getString('chromeChannel'),
        loadTimeData.getString('glicHeaderRequestTypes'));

    this.webClientStateListenerId =
        this.browserProxy.preloadPageCallbackRouter.webClientStateChanged
            .addListener((state: WebClientStateMojo) => {
              switch (state) {
                case WebClientStateMojo.kWarmed:
                  this.webClientState.assignAndSignal(WebClientState.WARMED);
                  break;
                case WebClientStateMojo.kResponsive:
                  this.persistentState.onClientReady();
                  this.webClientState.assignAndSignal(
                      WebClientState.RESPONSIVE);
                  break;
                case WebClientStateMojo.kUnresponsive:
                  this.webClientState.assignAndSignal(
                      WebClientState.UNRESPONSIVE);
                  break;
                case WebClientStateMojo.kError:
                  this.webClientState.assignAndSignal(WebClientState.ERROR);
                  this.destroyHost(WebClientState.ERROR);
                  break;
                case WebClientStateMojo.kUninitialized:
                  break;
                default:
                  assertNotReachedCase(state);
              }
            });

    this.webview.id = 'guestFrame';
    this.webview.setAttribute('partition', 'persist:glicpart');
    this.container.appendChild(this.webview);

    // Note that there is a migration underway to remove the use of webview.
    // Some elements of the webview frame are monitored from c++,
    // in chrome/browser/glic/host/glic_guest.cc. The elements tracked below
    // are more difficult to move right now, and will be moved to c++ when
    // migrating to a non-webview frame.

    // Keep here: sets focus which is a UI responsibility specific to the nested
    // frame.
    this.eventTracker.add(this.webview, 'loadstop', this.onLoadStop.bind(this));
    // Keep here: can be migrated using WebContentsDelegate only after migrating
    // away from webview.
    this.eventTracker.add(
        this.webview, 'newwindow', this.onNewWindow.bind(this));
    // Keep here: can be migrated using WebContentsDelegate only after migrating
    // away from webview.
    this.eventTracker.add(
        this.webview, 'permissionrequest', this.onPermissionRequest.bind(this));
    // Keep here: this is for logging only.
    this.eventTracker.add(this.webview, 'exit', this.onExit.bind(this));
    // Keep here: can be migrated using WebContentsDelegate only after migrating
    // away from webview.
    this.eventTracker.add(this.webview, 'zoomchange', (e: Event) => {
      if (!isZoomChangeEvent(e)) {
        return;
      }
      const percentage = Math.round(e.newZoomFactor * 100);
      const message = loadTimeData.getStringF('zoomLabel', percentage + '%');
      getAnnouncerInstance().announce(message);
      if (e.newZoomFactor > 0) {
        this.webview.getZoom((reportedZoom: number) => {
          this.displayScaleMultiplier = reportedZoom / e.newZoomFactor;
        });
      }
      this.browserProxy.pageHandler.onZoomLevelChange(e.newZoomFactor);
    });

    this.webview.src = this.persistentState.useLoadUrl();
  }

  getWebClientState(): ObservableValueReadOnly<WebClientState> {
    return this.webClientState;
  }

  focus(): void {
    this.webview.focus();
  }

  destroy() {
    if (this.webClientStateListenerId !== undefined) {
      this.browserProxy.preloadPageCallbackRouter.removeListener(
          this.webClientStateListenerId);
      this.webClientStateListenerId = undefined;
    }
    if (this.glicRequestHeaderInjector !== undefined) {
      this.glicRequestHeaderInjector.destroy();
      this.glicRequestHeaderInjector = undefined;
    }
    this.destroyHost(
        this.webClientState.getCurrentValue() === WebClientState.ERROR ?
            WebClientState.ERROR :
            WebClientState.UNINITIALIZED);
    this.eventTracker.removeAll();
    this.onDestroy.forEach(f => f());
    this.onDestroy = [];
    this.webview.remove();
  }

  // Destroys the host and prevents the host from being recreated. This results
  // in a webview which effectively cannot communicate with Chrome. Useful for
  // debugging.
  setDormant(): void {
    if (this.dormant) {
      return;
    }
    this.dormant = true;
    this.destroyHost();
  }

  private reportOnDestroy(): void {
    if (this.host) {
      let state = this.host.getDetailedWebClientState();
      if (state ===
          DetailedWebClientState
              .MOJO_PIPE_CLOSED_UNEXPECTEDLY_BEFORE_INITIALIZE) {
        state = DetailedWebClientState.BOOTSTRAP_PENDING;
      }
      chrome.histograms.recordEnumerationValue(
          'Glic.Host.WebClientState.OnDestroy', state,
          DetailedWebClientState.MAX_VALUE + 1);
    }
  }

  private destroyHost(
      webClientState?: WebClientState, isNavigationCommit: boolean = false) {
    if (this.host) {
      if (!isNavigationCommit) {
        this.reportOnDestroy();
      }
      this.host.destroy();
      this.host = undefined;
    }
    if (this.communicator) {
      this.communicator.destroy();
      this.communicator = undefined;
    }
    if (webClientState !== undefined) {
      this.webClientState.assignAndSignal(webClientState);
    }
  }

  zoom(zoomAction: ZoomAction) {
    const webview = this.webview;

    if (zoomAction === ZoomAction.kReset) {
      webview.setZoom(1.0);
      return;
    }

    webview.getZoom((reportedZoom: number) => {
      const activeZoom = reportedZoom / (this.displayScaleMultiplier || 1.0);
      const newFactor = zoomAction === ZoomAction.kZoomIn ?
          findNextZoomInFactor(activeZoom) :
          findNextZoomOutFactor(activeZoom);

      if (newFactor !== undefined) {
        webview.setZoom(newFactor);
      }
    });
  }

  waitingOnPanelWillOpen(): boolean {
    return this.host?.waitingOnPanelWillOpen() ?? false;
  }

  private onLoadStop(): void {
    if (this.webview.checkVisibility()) {
      this.webview.focus();
    }
  }

  private onNewWindow(e: chrome.webviewTag.NewWindowEvent): void {
    this.onNewWindowEvent(e);
  }

  private async onPermissionRequest(
      e: chrome.webviewTag.PermissionRequestEvent): Promise<void> {
    e.preventDefault();
    if (!this.host) {
      e.request.deny();
      return;
    }
    switch (e.permission) {
      case 'media': {
        // TODO(b/416092165): Block mic requests if the mic permission was not
        // granted.
        e.request.allow();
        return;
      }
      case 'geolocation': {
        const isGeolocationAllowed =
            await this.host.shouldAllowGeolocationPermissionRequest();
        if (isGeolocationAllowed) {
          e.request.allow();
        } else {
          e.request.deny();
        }
        return;
      }
      default:
        break;
    }
    console.warn(`Webview permission request was denied: ${e.permission}`);
    e.request.deny();
  }

  private onExit(event: chrome.webviewTag.ExitEvent): void {
    if (event.reason !== 'normal') {
      console.warn(`webview exit. processId: ${event.processId}, reason: ${
          event.reason}`);
    }
  }

  onGuestNavigationStarted(): void {}

  onGuestNavigated(
      url: string, isApiAllowed: boolean, pageType: GuestPageType,
      _isInitialCommit: boolean): void {
    if (this.dormant ||
        this.getWebClientState().getCurrentValue() === WebClientState.ERROR) {
      return;
    }

    const wasResponsive = this.getWebClientState().getCurrentValue() ===
        WebClientState.RESPONSIVE;

    if (this.host) {
      chrome.histograms.recordEnumerationValue(
          'Glic.Host.WebClientState.OnCommit',
          this.host.getDetailedWebClientState(),
          DetailedWebClientState.MAX_VALUE + 1);
      if (!wasResponsive) {
        this.reportOnDestroy();
      }
      this.destroyHost(
          /*webClientState=*/ undefined, /*isNavigationCommit=*/ true);
    }

    if (pageType !== GuestPageType.kRegular || !isApiAllowed) {
      this.delegate.webviewPageCommit(
          pageType === GuestPageType.kRegular ? GuestPageType.kLoadError :
                                                pageType,
          isApiAllowed);
      return;
    }

    const urlObj = URL.parse(url);
    const origin = urlObj ? urlObj.origin : '*';
    const contentWindow =
        this.webview.contentWindow || (window as unknown as WindowProxy);
    this.communicator = new GlicApiCommunicator(origin, contentWindow);
    this.host = new GlicApiHost(this.browserProxy, this.communicator);
    this.host.getWebClientState().subscribe(state => {
      if (state === WebClientState.ERROR) {
        this.webClientState.assignAndSignal(WebClientState.ERROR);
        this.destroyHost(WebClientState.ERROR);
      }
    });
    this.communicator.contentLoaded();

    this.browserProxy.pageHandler.webviewCommitted(url);

    if (wasResponsive) {
      this.persistentState.onCommitAfterConnect(url);
    }

    if (!_isInitialCommit) {
      this.delegate.webviewPageCommit(GuestPageType.kRegular, isApiAllowed);
    }
  }

  private onNewWindowEvent(event: chrome.webviewTag.NewWindowEvent) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.host) {
      return;
    }

    if (loadTimeData.getBoolean('glicPopupWindowsEnabled') &&
        event.windowOpenDisposition === 'new_popup') {
      this.host.openLinkInPopup(
          event.targetUrl, event.initialWidth, event.initialHeight);
    } else {
      this.host.openLinkInNewTab(event.targetUrl);
    }
  }

  getApiHost(): GlicApiHost|undefined {
    return this.host;
  }
}

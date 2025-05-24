// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import type {Subscriber} from './glic_api/glic_api.js';
import {DetailedWebClientState, GlicApiHost, WebClientState} from './glic_api_impl/glic_api_host.js';
import type {ApiHostEmbedder} from './glic_api_impl/glic_api_host.js';
import {ObservableValue} from './observable.js';
import type {ObservableValueReadOnly} from './observable.js';
import {OneShotTimer} from './timer.js';

export type PageType =
    // A login page.
    'login'
    // A page that should be displayed.
    |'regular'
    // A error page that should be displayed.
    |'guestError';

// Calls from the webview to its owner.
export interface WebviewDelegate {
  // Called when there is an error during page load.
  webviewError(reason: string): void;
  // Called when the embedded web page is unresponsive.
  webviewUnresponsive(): void;
  // Called when a page commits inside the webview.
  webviewPageCommit(pageType: PageType): void;
}

// To match needed pieces of tools/typescript/definitions/web_request.d.ts,
// because this enum isn't actually available in this context.
enum ResourceType {
  MAIN_FRAME = 'main_frame',
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

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

// Creates and manages the <webview> element, and the GlicApiHost which
// communicates with it.
export class WebviewController {
  webview: chrome.webviewTag.WebView;
  private host?: GlicApiHost;
  private hostSubscriber?: Subscriber;
  private onDestroy: Array<() => void> = [];
  private eventTracker = new EventTracker();
  private webClientState =
      ObservableValue.withValue(WebClientState.UNINITIALIZED);
  private oneMinuteTimer = new OneShotTimer(1000 * 60);

  constructor(
      private readonly container: HTMLElement,
      private browserProxy: BrowserProxyImpl,
      private delegate: WebviewDelegate,
      private hostEmbedder: ApiHostEmbedder,
      private persistentState: WebviewPersistentState,
  ) {
    this.webview =
        document.createElement('webview') as chrome.webviewTag.WebView;

    // Intercept all main frame requests, and block them if they are not allowed
    // origins.
    const onBeforeRequest = this.onBeforeRequest.bind(this);
    this.webview.request.onBeforeRequest.addListener(
        onBeforeRequest, {
          types: [ResourceType.MAIN_FRAME],
          urls: ['<all_urls>'],
        },
        ['blocking']);
    this.onDestroy.push(() => {
      this.webview.request.onBeforeRequest.removeListener(onBeforeRequest);
    });
    const onBeforeSendHeaders = this.onBeforeSendHeaders.bind(this);
    this.webview.request.onBeforeSendHeaders.addListener(
        onBeforeSendHeaders, {
          types: [ResourceType.MAIN_FRAME],
          urls: ['<all_urls>'],
        },
        ['blocking', 'requestHeaders']);
    this.onDestroy.push(() => {
      this.webview.request.onBeforeSendHeaders.removeListener(
          onBeforeSendHeaders);
    });

    this.webview.id = 'guestFrame';
    this.webview.setAttribute('partition', 'persist:glicpart');
    this.container.appendChild(this.webview);

    this.eventTracker.add(
        this.webview, 'loadcommit', this.onLoadCommit.bind(this));
    this.eventTracker.add(
        this.webview, 'contentload', this.contentLoaded.bind(this));
    this.eventTracker.add(this.webview, 'loadstop', this.onLoadStop.bind(this));
    this.eventTracker.add(
        this.webview, 'newwindow', this.onNewWindow.bind(this));
    this.eventTracker.add(
        this.webview, 'permissionrequest', this.onPermissionRequest.bind(this));
    this.eventTracker.add(
        this.webview, 'unresponsive', this.onUnresponsive.bind(this));
    this.eventTracker.add(this.webview, 'exit', this.onExit.bind(this));

    this.webview.src = this.persistentState.useLoadUrl();

    this.oneMinuteTimer.start(() => {
      if (this.host) {
        chrome.metricsPrivate.recordEnumerationValue(
            'Glic.Host.WebClientState.AtOneMinute',
            this.host.getDetailedWebClientState(),
            DetailedWebClientState.MAX_VALUE + 1);
      }
    });
  }

  getWebClientState(): ObservableValueReadOnly<WebClientState> {
    return this.webClientState;
  }

  destroy() {
    this.oneMinuteTimer.reset();
    if (this.host) {
      chrome.metricsPrivate.recordEnumerationValue(
          'Glic.Host.WebClientState.OnDestroy',
          this.host.getDetailedWebClientState(),
          DetailedWebClientState.MAX_VALUE + 1);
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

  private destroyHost(webClientState: WebClientState) {
    if (this.hostSubscriber) {
      this.hostSubscriber.unsubscribe();
      this.hostSubscriber = undefined;
    }
    if (this.host) {
      this.host.destroy();
      this.host = undefined;
    }
    this.webClientState.assignAndSignal(webClientState);
  }

  waitingOnPanelWillOpen(): boolean {
    return this.host?.waitingOnPanelWillOpen() ?? false;
  }

  private onLoadCommit(e: any): void {
    this.loadCommit(e.url, e.isTopLevel);
  }

  private onLoadStop(): void {
    this.webview.focus();
  }

  private onNewWindow(e: Event): void {
    this.onNewWindowEvent(e as chrome.webviewTag.NewWindowEvent);
  }

  private async onPermissionRequest(e: any): Promise<void> {
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
    }
    console.warn(`Webview permission request was denied: ${e.permission}`);
    e.request.deny();
  }

  private onUnresponsive(): void {
    this.delegate.webviewUnresponsive();
  }

  private onExit(e: any): void {
    if (e.reason !== 'normal') {
      this.destroyHost(WebClientState.ERROR);
      chrome.metricsPrivate.recordUserAction('GlicSessionWebClientCrash');
      console.warn(`webview exit. reason: ${e.reason}`);
    }
  }

  private loadCommit(url: string, isTopLevel: boolean) {
    if (!isTopLevel) {
      return;
    }
    if (this.host) {
      chrome.metricsPrivate.recordEnumerationValue(
          'Glic.Host.WebClientState.OnCommit',
          this.host.getDetailedWebClientState(),
          DetailedWebClientState.MAX_VALUE + 1);
    }
    const wasResponsive = this.getWebClientState().getCurrentValue() ===
        WebClientState.RESPONSIVE;

    this.destroyHost(WebClientState.UNINITIALIZED);

    if (this.webview.contentWindow) {
      this.host = new GlicApiHost(
          this.browserProxy, this.webview.contentWindow, new URL(url).origin,
          this.hostEmbedder);
      this.hostSubscriber = this.host.getWebClientState().subscribe(state => {
        if (state === WebClientState.RESPONSIVE) {
          this.persistentState.onClientReady();
        }
        this.webClientState.assignAndSignal(state);
      });
    }
    this.browserProxy.handler.webviewCommitted({url});

    // TODO(https://crbug.com/388328847): Remove when login issues are resolved.
    if (url.startsWith('https://login.corp.google.com/') ||
        url.startsWith('https://accounts.google.com/') ||
        url.startsWith('https://accounts.googlers.com/') ||
        url.startsWith('https://gaiastaging.corp.google.com/')) {
      this.delegate.webviewPageCommit('login');
    } else if (new URL(url).pathname.startsWith('/sorry/')) {
      this.delegate.webviewPageCommit('guestError');
    } else {
      if (wasResponsive) {
        this.persistentState.onCommitAfterConnect(url);
      }
      this.delegate.webviewPageCommit('regular');
    }
  }

  private contentLoaded() {
    if (this.host) {
      this.host.contentLoaded();
    }
  }

  private onNewWindowEvent(event: chrome.webviewTag.NewWindowEvent) {
    if (!this.host) {
      return;
    }
    event.preventDefault();
    this.host.openLinkInNewTab(event.targetUrl);
    event.stopPropagation();
  }

  private onBeforeRequest:
      ChromeEventFunctionType<typeof chrome.webRequest.onBeforeRequest> =
          (details) => {
            // Allow subframe requests.
            if (details.frameId !== 0) {
              return {};
            }
            return {cancel: !urlMatchesAllowedOrigin(details.url)};
          };

  // Attaches the X-Glic headers to all main-frame requests.
  // X-Glic: 1
  // X-Glic-Chrome-Channel: stable
  // X-Glic-Chrome-Version: 137.0.1234.0
  private onBeforeSendHeaders:
      ChromeEventFunctionType<typeof chrome.webRequest.onBeforeSendHeaders> =
          (details) => {
            // Ignore subframe requests.
            if (details.frameId !== 0) {
              return {};
            }
            const requestHeaders = details.requestHeaders || [];
            requestHeaders.push({
              name: 'X-Glic',
              value: '1',
            });
            requestHeaders.push({
              name: 'X-Glic-Chrome-Version',
              value: loadTimeData.getString('chromeVersion'),
            });
            requestHeaders.push({
              name: 'X-Glic-Chrome-Channel',
              value: loadTimeData.getString('chromeChannel'),
            });
            return {requestHeaders};
          };
}

/**
 * Returns a URLPattern given an origin pattern string that has the syntax:
 * <protocol>://<hostname>[:<port>]
 * where <protocol>, <hostname> and <port> are inserted into URLPattern.
 */
export function matcherForOrigin(originPattern: string): URLPattern|null {
  // This regex is overly permissive in what characters can exist in protocol
  // or hostname. This isn't a problem because we're just passing data to
  // URLPattern.
  const match = originPattern.match(/([^:]+):\/\/([^:]*)(?::(\d+))?[/]?/);
  if (!match) {
    return null;
  }

  const [protocol, hostname, port] = [match[1], match[2], match[3] ?? '*'];
  try {
    return new URLPattern({protocol, hostname, port});
  } catch (_) {
    return null;
  }
}

export function urlMatchesAllowedOrigin(url: string) {
  // For development.
  if (loadTimeData.getBoolean('devMode')) {
    return true;
  }

  // A URL is allowed if it either matches glicGuestURL's origin, or it matches
  // any of the approved origins.
  const defaultUrl = new URL(loadTimeData.getString('glicGuestURL'));
  if (matcherForOrigin(defaultUrl.origin)?.test(url)) {
    return true;
  }

  return loadTimeData.getString('glicAllowedOrigins')
      .split(' ')
      .some(origin => matcherForOrigin(origin.trim())?.test(url));
}

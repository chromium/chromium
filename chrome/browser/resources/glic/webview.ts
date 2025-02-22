// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxyImpl} from './browser_proxy.js';
import type {ApiHostEmbedder} from './glic_api_impl/glic_api_host.js';
import {GlicApiHost} from './glic_api_impl/glic_api_host.js';

export type PageType =
    // A login page.
    'login'
    // A page that should be displayed.
    |'regular';

// Calls from the webview to its owner.
export interface WebviewDelegate {
  // Called when there is an error during page load.
  webviewError(): void;
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

// Creates and manages the <webview> element, and the GlicApiHost which
// communicates with it.
export class WebviewController {
  webview: chrome.webviewTag.WebView;
  private host?: GlicApiHost;
  private onDestroy: Array<() => void> = [];
  private eventTracker = new EventTracker();

  constructor(
      private readonly container: HTMLElement,
      private browserProxy: BrowserProxyImpl,
      private delegate: WebviewDelegate,
      private hostEmbedder: ApiHostEmbedder,
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

    this.webview.src = loadTimeData.getString('glicGuestURL');
  }

  destroy() {
    if (this.host) {
      this.host.destroy();
      this.host = undefined;
    }
    this.eventTracker.removeAll();
    this.onDestroy.forEach(f => f());
    this.onDestroy = [];
    this.webview.remove();
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

  private onPermissionRequest(e: any): void {
    if (e.permission === 'media' || e.permission === 'geolocation') {
      e.request.allow();
    }
  }

  private onUnresponsive(): void {
    this.delegate.webviewUnresponsive();
  }

  private onExit(e: any): void {
    if (e.reason !== 'normal') {
      this.delegate.webviewError();
    }
  }

  private loadCommit(url: string, isTopLevel: boolean) {
    if (!isTopLevel) {
      return;
    }
    if (this.host) {
      this.host.destroy();
      this.host = undefined;
    }

    if (this.webview.contentWindow) {
      this.host = new GlicApiHost(
          this.browserProxy, this.webview.contentWindow, new URL(url).origin,
          this.hostEmbedder);
    }
    this.browserProxy.handler.webviewCommitted({url});

    // TODO(https://crbug.com/388328847): Remove when login issues are resolved.
    if (url.startsWith('https://login.corp.google.com/') ||
        url.startsWith('https://accounts.google.com/')) {
      this.delegate.webviewPageCommit('login');
    } else {
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

  private onBeforeRequest(details: any) {
    // Allow subframe requests.
    if (details.frameId !== 0) {
      return {};
    }
    return {cancel: !urlMatchesAllowedOrigin(details.url)};
  }
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
  if (loadTimeData.getBoolean('glicSkipOriginCheck')) {
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

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import type {ClickInfo} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {Command} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {ScrollDepth} from './whats_new.mojom-webui.js';
import {getCss} from './whats_new_app.css.js';
import {getHtml} from './whats_new_app.html.js';
import {WhatsNewProxyImpl} from './whats_new_proxy.js';

enum EventType {
  BROWSER_COMMAND = 'browser_command',
  PAGE_LOADED = 'page_loaded',
  MODULE_IMPRESSION = 'module_impression',
  EXPLORE_MORE_OPEN = 'explore_more_open',
  EXPLORE_MORE_CLOSE = 'explore_more_close',
  SCROLL = 'scroll',
  TIME_ON_PAGE_MS = 'time_on_page_ms',
  GENERAL_LINK_CLICK = 'general_link_click',
}

// TODO(crbug.com/342172972): Remove legacy browser command format.
interface LegacyBrowserCommandData {
  commandId: number;
  clickInfo: ClickInfo;
}

interface BrowserCommandData {
  event: EventType.BROWSER_COMMAND;
  commandId: number;
  clickInfo: ClickInfo;
}

interface PageLoadedMetric {
  event: EventType.PAGE_LOADED;
  type: 'version';
  version: number;
}

interface ModuleImpressionMetric {
  event: EventType.MODULE_IMPRESSION;
  module_name: string;
}

interface ExploreMoreOpenMetric {
  event: EventType.EXPLORE_MORE_OPEN;
  module_name: 'archive';
}

interface ExploreMoreCloseMetric {
  event: EventType.EXPLORE_MORE_CLOSE;
  module_name: 'archive';
}

interface ScrollDepthMetric {
  event: EventType.SCROLL;
  percent_scrolled: 25|50|75|100;
}

interface TimeOnPageMetric {
  event: EventType.TIME_ON_PAGE_MS;
  time: number;
}

interface GeneralLinkClickMetric {
  event: EventType.GENERAL_LINK_CLICK;
  link_text: string;
  link_type: string;
  link_url: string;
  module_name: string;
}

type BrowserCommand = LegacyBrowserCommandData|BrowserCommandData;
type MetricData = PageLoadedMetric|ModuleImpressionMetric|ExploreMoreOpenMetric|
    ExploreMoreCloseMetric|ScrollDepthMetric|TimeOnPageMetric|
    GeneralLinkClickMetric;

interface EventData {
  data: BrowserCommand|MetricData;
}

// Narrow the type of the message data. This is necessary for the
// legacy message format that does not supply an event name.
function isBrowserCommand(messageData: BrowserCommand|
                          MetricData): messageData is BrowserCommand {
  // TODO(crbug.com/342172972): Remove legacy browser command format checks.
  if (Object.hasOwn(messageData, 'event')) {
    return (messageData as BrowserCommandData | MetricData).event ===
        EventType.BROWSER_COMMAND;
  } else {
    return Object.hasOwn(messageData, 'commandId');
  }
}

function handleBrowserCommand(messageData: BrowserCommand) {
  if (!Object.values(Command).includes(messageData.commandId)) {
    return;
  }
  const {commandId} = messageData;
  const handler = BrowserCommandProxy.getInstance().handler;
  handler.canExecuteCommand(commandId).then(({canExecute}) => {
    if (canExecute) {
      handler.executeCommand(commandId, messageData.clickInfo);
    } else {
      console.warn('Received invalid command: ' + commandId);
    }
  });
}

function handlePageLoadMetric(data: PageLoadedMetric, isAutoOpen: boolean) {
  if (data.type === 'version' && Number.isInteger(data.version)) {
    const {handler} = WhatsNewProxyImpl.getInstance();
    handler.recordVersionPageLoaded(isAutoOpen);
  } else {
    console.warn(
        'Unrecognized page version: ' + data.type + ', ' + data.version);
  }
}

function handleScrollDepthMetric(data: ScrollDepthMetric) {
  let scrollDepth;
  switch (data.percent_scrolled) {
    case 25:
      scrollDepth = ScrollDepth.k25;
      break;
    case 50:
      scrollDepth = ScrollDepth.k50;
      break;
    case 75:
      scrollDepth = ScrollDepth.k75;
      break;
    case 100:
      scrollDepth = ScrollDepth.k100;
      break;
  }
  if (scrollDepth) {
    const {handler} = WhatsNewProxyImpl.getInstance();
    handler.recordScrollDepth(scrollDepth);
  } else {
    console.warn('Unrecognized scroll percentage: ', data.percent_scrolled);
  }
}

function handleTimeOnPageMetric(data: TimeOnPageMetric) {
  if (Number.isInteger(data.time) && data.time > 0) {
    const {handler} = WhatsNewProxyImpl.getInstance();
    const delta: TimeDelta = {microseconds: BigInt(data.time)};
    handler.recordTimeOnPage(delta);
  } else {
    console.warn('Invalid time: ', data.time);
  }
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
    };
  }

  protected url_: string = '';

  private isAutoOpen_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    const queryParams = new URLSearchParams(window.location.search);
    this.isAutoOpen_ = queryParams.has('auto');

    // There are no subpages in What's New. Also remove the query param here
    // since its value is recorded.
    window.history.replaceState(undefined /* stateObject */, '', '/');
  }

  override connectedCallback() {
    super.connectedCallback();

    WhatsNewProxyImpl.getInstance().handler.getServerUrl().then(
        ({url}: {url: Url}) => this.handleUrlResult_(url.url));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /**
   * Called when embedded content has loaded.
   */
  protected onContentLoaded_() {
    const {handler} = WhatsNewProxyImpl.getInstance();
    handler.recordTimeToLoadContent(Date.now());
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
    this.url_ = url.concat(`latest=${latest}`);

    this.eventTracker_.add(
        window, 'message',
        (event: Event) => this.handleMessage_(event as MessageEvent));
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

    if (isBrowserCommand(data)) {
      handleBrowserCommand(data);
      return;
    }

    const {handler} = WhatsNewProxyImpl.getInstance();
    switch (data.event) {
      case EventType.PAGE_LOADED:
        handlePageLoadMetric(data, this.isAutoOpen_);
        break;
      case EventType.MODULE_IMPRESSION:
        handler.recordModuleImpression(data.module_name);
        break;
      case EventType.EXPLORE_MORE_OPEN:
        handler.recordExploreMoreToggled(true);
        break;
      case EventType.EXPLORE_MORE_CLOSE:
        handler.recordExploreMoreToggled(false);
        break;
      case EventType.SCROLL:
        handleScrollDepthMetric(data);
        break;
      case EventType.TIME_ON_PAGE_MS:
        handleTimeOnPageMetric(data);
        break;
      case EventType.GENERAL_LINK_CLICK:
        handler.recordModuleLinkClicked(data.module_name);
        break;
      default:
        console.warn('Unrecognized message.', data);
    }
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import type {ClickInfo} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {Command} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {JSTime, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {ModulePosition, ScrollDepth} from './whats_new.mojom-webui.js';
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
  MODULES_RENDERED = 'modules_rendered',
}

enum SectionType {
  SPOTLIGHT = 'spotlight',
  EXPLORE_MORE = 'explore_more',
}

// Used to map a section and order value to the ModulePosition mojo type.
const kModulePositionsMap: Record<SectionType, ModulePosition[]> = {
  [SectionType.SPOTLIGHT]: [
    ModulePosition.kSpotlight1,
    ModulePosition.kSpotlight2,
    ModulePosition.kSpotlight3,
    ModulePosition.kSpotlight4,
  ],
  [SectionType.EXPLORE_MORE]: [
    ModulePosition.kExploreMore1,
    ModulePosition.kExploreMore2,
    ModulePosition.kExploreMore3,
    ModulePosition.kExploreMore4,
    ModulePosition.kExploreMore5,
    ModulePosition.kExploreMore6,
  ],
};

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

interface VersionPageLoadedMetric {
  event: EventType.PAGE_LOADED;
  type: 'version';
  version: number;
  page_uid?: string;
}

interface EditionPageLoadedMetric {
  event: EventType.PAGE_LOADED;
  type: 'edition';
  version: null;
  page_uid: string;
}

interface ModuleImpressionMetric {
  event: EventType.MODULE_IMPRESSION;
  module_name?: string;
  section?: SectionType;
  order?: '1'|'2'|'3'|'4'|'5'|'6';
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
  percent_scrolled: '25'|'50'|'75'|'100';
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
  module_name?: string;
  section?: SectionType;
  order?: '1'|'2'|'3'|'4'|'5'|'6';
}

interface ModulesRenderedMetric {
  event: EventType.MODULES_RENDERED;
}

type PageLoadedMetric = VersionPageLoadedMetric|EditionPageLoadedMetric;
type BrowserCommand = LegacyBrowserCommandData|BrowserCommandData;
type MetricData = PageLoadedMetric|ModuleImpressionMetric|ExploreMoreOpenMetric|
    ExploreMoreCloseMetric|ScrollDepthMetric|TimeOnPageMetric|
    GeneralLinkClickMetric|ModulesRenderedMetric;

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
      const pageHandler = WhatsNewProxyImpl.getInstance().handler;
      pageHandler.recordBrowserCommandExecuted();
    } else {
      console.warn('Received invalid command: ' + commandId);
    }
  });
}

function handlePageLoadMetric(data: PageLoadedMetric, isAutoOpen: boolean) {
  const {handler} = WhatsNewProxyImpl.getInstance();
  const now: JSTime = {msec: Date.now()};
  handler.recordTimeToLoadContent(now);

  // Record initial scroll depth as 0%.
  handler.recordScrollDepth(ScrollDepth.k0);

  switch (data.type) {
    case 'version':
      if (Number.isInteger(data.version)) {
        const {handler} = WhatsNewProxyImpl.getInstance();
        handler.recordVersionPageLoaded(isAutoOpen);
      }
      break;
    case 'edition':
      const {handler} = WhatsNewProxyImpl.getInstance();
      handler.recordEditionPageLoaded(data.page_uid, isAutoOpen);
      break;
    default:
      console.warn('Unrecognized page version: ' + (data as any)!.type);
  }
}

function handleScrollDepthMetric(data: ScrollDepthMetric) {
  let scrollDepth;
  // Embedded page never sends scroll depth 0%. This value is created in
  // handlePageLoadMetric instead.
  switch (data.percent_scrolled) {
    case '25':
      scrollDepth = ScrollDepth.k25;
      break;
    case '50':
      scrollDepth = ScrollDepth.k50;
      break;
    case '75':
      scrollDepth = ScrollDepth.k75;
      break;
    case '100':
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

// Parse `section` and `order` values from the untrusted source.
function parseOrder(
    section: string|undefined, order: string|undefined): ModulePosition {
  // Reject messages that send falsy `section` or `order` values.
  if (!section || !order) {
    return ModulePosition.kUndefined;
  }

  // Ensure `section` is one of the defined enum values.
  if (!(Object.values(SectionType).includes(section as SectionType))) {
    return ModulePosition.kUndefined;
  }

  const moduleSection = kModulePositionsMap[section as SectionType];
  const orderAsNumber = Number.parseInt(order, 10);
  // Ensure `order` is a number within the 1-based range of the given section.
  if (Number.isNaN(orderAsNumber) || orderAsNumber > moduleSection.length ||
      orderAsNumber < 1) {
    return ModulePosition.kUndefined;
  }

  // Get ModulePosition enum from validated message parameters.
  return moduleSection[orderAsNumber - 1] as ModulePosition;
}

// Replace first letter with its uppercase equivalent.
function uppercaseFirstLetter(word: string) {
  return word.replace(/^\w/, firstLetter => firstLetter.toUpperCase());
}

// Convert kebab-case string (e.g. my-module-name) to PascalCase (e.g.
// MyModuleName).
function kebabCaseToCamelCase(input: string) {
  return input
      // Split on hyphen to remove it.
      .split('-')
      // Uppercase first letter of each word.
      .map(uppercaseFirstLetter)
      // Join back into contiguous string.
      .join('');
}

// Convert legacy module names (i.e. module uid) to a format that can
// be captured in metrics.
//
// Previous module names were created in the format `NNN-module-name`.
// These module names cannot be recorded in metrics as-is, due to the
// hyphens. They must be switched to a PascalCase format. New module
// names will not follow this format, therefore will be ignored if they
// don't contain a hyphen.
//
// Exported for testing purposes only.
export function formatModuleName(moduleName: string) {
  if (!moduleName.includes('-')) {
    return moduleName;
  }
  // Remove the 3 numbers at the beginning of the name (`NNN-`)
  const withoutPrefix = moduleName.replace(/^\d{3}-/, '');
  return kebabCaseToCamelCase(withoutPrefix);
}

function handleModuleImpression(data: ModuleImpressionMetric) {
  // Reject falsy `module_name`, including empty strings.
  if (!data.module_name) {
    return;
  }
  const position = parseOrder(data.section, data.order);
  const {handler} = WhatsNewProxyImpl.getInstance();
  handler.recordModuleImpression(formatModuleName(data.module_name), position);
}

function handleModuleLinkClicked(data: GeneralLinkClickMetric) {
  // Reject falsy `module_name`, including empty strings.
  if (!data.module_name) {
    return;
  }
  const position = parseOrder(data.section, data.order);
  const {handler} = WhatsNewProxyImpl.getInstance();
  handler.recordModuleLinkClicked(formatModuleName(data.module_name), position);
}

function handleTimeOnPageMetric(data: TimeOnPageMetric) {
  if (Number.isInteger(data.time) && data.time > 0) {
    const {handler} = WhatsNewProxyImpl.getInstance();
    // Event contains time in milliseconds. Convert to microseconds.
    const delta: TimeDelta = {microseconds: BigInt(data.time) * 1000n};
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

    // Indicates this tab was added automatically by the browser.
    this.isAutoOpen_ = queryParams.has('auto');

    // There are no subpages in What's New. Also remove the query param here
    // since its value is recorded.
    window.history.replaceState(undefined /* stateObject */, '', '/');
  }

  override connectedCallback() {
    super.connectedCallback();

    WhatsNewProxyImpl.getInstance()
        .handler.getServerUrl(loadTimeData.getBoolean('isStaging'))
        .then(({url}: {url: Url}) => this.handleUrlResult_(url.url));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
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
    if (loadTimeData.getBoolean('isWhatsNewV2')) {
      // The browser has auto-opened the page due to an upgrade.
      // Let the embedded page know to display the "up to date" banner.
      this.url_ = url.concat(`updated=${latest}`);
    } else {
      // The latest version of the page is being shown. Do not redirect.
      this.url_ = url.concat(`latest=${latest}`);
    }

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
        handleModuleImpression(data);
        break;
      case EventType.MODULES_RENDERED:
        // Ignored.
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
        handleModuleLinkClicked(data);
        break;
      default:
        console.warn('Unrecognized message.', data);
    }
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);

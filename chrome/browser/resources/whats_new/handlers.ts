// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Command} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import type {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {formatModuleName} from './format_module_name.js';
import {EventType, kModulePositionsMap, SectionType} from './types.js';
import type {BrowserCommand, ModuleEvent, PageLoadedMetric, ScrollDepthMetric} from './types.js';
import {ModulePosition, ScrollDepth} from './whats_new.mojom-webui.js';
import {WhatsNewProxyImpl} from './whats_new_proxy.js';

export function handleBrowserCommand(messageData: BrowserCommand) {
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

export function handlePageLoadMetric(
    data: PageLoadedMetric, isAutoOpen: boolean) {
  const {handler} = WhatsNewProxyImpl.getInstance();
  const now = new Date();
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

export function handleScrollDepthMetric(data: ScrollDepthMetric) {
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
    default:
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

export function handleModuleEvent(data: ModuleEvent) {
  // Reject falsy `module_name`, including empty strings.
  if (!data.module_name) {
    return;
  }
  const position = parseOrder(data.section, data.order);
  const {handler} = WhatsNewProxyImpl.getInstance();
  switch (data.event) {
    case EventType.MODULE_IMPRESSION:
      handler.recordModuleImpression(
          formatModuleName(data.module_name), position);
      break;
    case EventType.GENERAL_LINK_CLICK:
      handler.recordModuleLinkClicked(
          formatModuleName(data.module_name), position);
      break;
    case EventType.VIDEO_STARTED:
      handler.recordModuleVideoStarted(
          formatModuleName(data.module_name), position);
      break;
    case EventType.VIDEO_ENDED:
      handler.recordModuleVideoEnded(
          formatModuleName(data.module_name), position);
      break;
    case EventType.PLAY_CLICKED:
      handler.recordModulePlayClicked(
          formatModuleName(data.module_name), position);
      break;
    case EventType.PAUSE_CLICKED:
      handler.recordModulePauseClicked(
          formatModuleName(data.module_name), position);
      break;
    case EventType.RESTART_CLICKED:
      handler.recordModuleRestartClicked(
          formatModuleName(data.module_name), position);
      break;
    case EventType.EXPEND_MEDIA:
      WhatsNewProxyImpl.getInstance().handler.recordExpandMediaToggled(
          data.module_name, true);
      break;
    case EventType.CLOSE_EXPANDED_MEDIA:
      WhatsNewProxyImpl.getInstance().handler.recordExpandMediaToggled(
          data.module_name, false);
      break;
    default:
      break;
  }
}

// Handle the two types of time_on_page metrics.
export function handleTimeOnPageMetric(time: number, isHeartbeat: boolean) {
  if (Number.isInteger(time) && time > 0) {
    const {handler} = WhatsNewProxyImpl.getInstance();
    // Event contains time in milliseconds. Convert to microseconds.
    const delta: TimeDelta = {microseconds: BigInt(time) * 1000n};
    handler.recordTimeOnPage(delta, isHeartbeat);
  } else {
    console.warn('Invalid time: ', time);
  }
}

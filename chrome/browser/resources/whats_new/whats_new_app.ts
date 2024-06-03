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
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './whats_new_app.css.js';
import {getHtml} from './whats_new_app.html.js';
import {WhatsNewProxyImpl} from './whats_new_proxy.js';

enum EventType {
  BROWSER_COMMAND = 'browser_command',
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

type BrowserCommand = LegacyBrowserCommandData|BrowserCommandData;

interface EventData {
  data: BrowserCommand;
}

// Narrow the type of the message data. This is necessary for the
// legacy message format that does not supply an event name.
function isBrowserCommand(messageData: LegacyBrowserCommandData|
                          BrowserCommandData): messageData is BrowserCommand {
  // TODO(crbug.com/342172972): Remove legacy browser command format checks.
  if (Object.hasOwn(messageData, 'event')) {
    return (messageData as BrowserCommandData).event ===
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
    } else {
      console.warn('Received invalid message');
    }
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);

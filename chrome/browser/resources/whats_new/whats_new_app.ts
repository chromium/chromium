// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './whats_new_error_page.js';
import './strings.m.js';

import {ClickInfo, Command} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WhatsNewProxyImpl} from './whats_new_proxy.js';

type CommandData = {
  commandId: number,
  clickInfo: ClickInfo,
};

// TODO (https://www.crbug.com/1219381): Add some additional parameters so
// that we can filter the messages a bit better.
type BrowserCommandMessageData = {
  data: CommandData,
};

const WhatsNewAppElementBase = WebUIListenerMixin(PolymerElement);

export class WhatsNewAppElement extends WhatsNewAppElementBase {
  static get is() {
    return 'whats-new-app';
  }

  static get properties() {
    return {
      showErrorPage_: {
        type: Boolean,
        value: false,
      },

      showFeedbackButton_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showFeedbackButton'),
      },

      // Whether the page should show the iframe and handle messages from it.
      canHandleMessages_: {
        type: Boolean,
        value: false,
      },

      url_: {
        type: String,
        value: '',
      }
    };
  }

  private canHandleMessages_: boolean;
  private showErrorPage_: boolean;
  private showFeedbackButton_: boolean;
  private url_: string;

  private isAutoOpen_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'preload-url', (url: string) => this.handleUrlResult_(url, false));
    const queryParams = new URLSearchParams(window.location.search);
    this.isAutoOpen_ = queryParams.has('auto') && !isChromeOS;
    WhatsNewProxyImpl.getInstance()
        .initialize(this.isAutoOpen_)
        .then(url => this.handleUrlResult_(url, true));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /**
   * Handles the URL result of sending the initialize WebUI message. An initial
   * URL will be sent back via the preload-url event, and the final URL is
   * sent when the callback resolves.
   * @param url The What's New URL to use in the iframe.
   * @param canHandleMessages Whether the page should begin handling messages
   *     from the iframe. True only if |url| is the final URL to load.
   */
  private handleUrlResult_(url: string|null, canHandleMessages: boolean) {
    if (!url) {
      this.url_ = '';
      this.showErrorPage_ = true;
      return;
    }

    const latest = this.isAutoOpen_ ? 'true' : 'false';
    const feedback = this.showFeedbackButton_ ? 'true' : 'false';
    url += url.includes('?') ? '&' : '?';
    this.url_ = url.concat(`latest=${latest}&feedback=${feedback}`);

    this.canHandleMessages_ = canHandleMessages;

    if (canHandleMessages) {
      this.eventTracker_.add(
          window, 'message',
          event => this.handleMessage_(event as MessageEvent));
    }
  }

  private handleMessage_(event: MessageEvent) {
    if (!this.canHandleMessages_) {
      return;
    }

    const {data, origin} = event;
    const iframeUrl = new URL(this.url_);
    if (!data || origin !== iframeUrl.origin) {
      return;
    }

    const commandData = (data as BrowserCommandMessageData).data;

    const commandId = Object.values(Command).includes(commandData.commandId) ?
        commandData.commandId :
        Command.kUnknownCommand;

    const handler = BrowserCommandProxy.getInstance().handler;
    handler.canExecuteCommand(commandId).then(({canExecute}) => {
      if (canExecute) {
        handler.executeCommand(commandId, commandData.clickInfo);
      } else {
        console.warn('Received invalid command: ' + commandId);
      }
    });
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);

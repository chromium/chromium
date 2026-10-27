// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';
import {WindowProxy} from './window_proxy.js';

const DEFAULT_EXPANDED_LHS_WIDTH_PX = 261;

export enum NtpLhsMessageType {
  NTP_LHS_READY = 'NTP_LHS_READY',
  NTP_LHS_CLOSE = 'NTP_LHS_CLOSE',
  NTP_LHS_NEW_THREAD = 'NTP_LHS_NEW_THREAD',
  NTP_LHS_NAVIGATE = 'NTP_LHS_NAVIGATE',
}

export interface NtpLhsReadyMessage {
  type: NtpLhsMessageType.NTP_LHS_READY;
  width?: number;
}

export interface NtpLhsCloseMessage {
  type: NtpLhsMessageType.NTP_LHS_CLOSE;
}

export interface NtpLhsNewThreadMessage {
  type: NtpLhsMessageType.NTP_LHS_NEW_THREAD;
}

export interface NtpLhsNavigateMessage {
  type: NtpLhsMessageType.NTP_LHS_NAVIGATE;
  url: string;
}

export type SearchToChromeLhsMessage = NtpLhsReadyMessage|NtpLhsCloseMessage|
    NtpLhsNewThreadMessage|NtpLhsNavigateMessage;

export class LeftHandSideElement extends CrLitElement {
  static get is() {
    return 'cr-left-hand-side';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableThreadsRailHybrid_: {type: Boolean},
      expandedLhsUrl_: {type: String},
      expandedWidth_: {type: Number},
      isDark: {type: Boolean},
      isExpanded_: {type: Boolean},
      isExpandedReady_: {type: Boolean},
    };
  }

  accessor isDark: boolean|undefined = undefined;
  protected accessor enableThreadsRailHybrid_: boolean =
      loadTimeData.getBoolean('enableThreadsRailHybrid');
  protected accessor expandedLhsUrl_: string =
      loadTimeData.getString('expandedLhsUrl');
  protected accessor expandedWidth_: number = DEFAULT_EXPANDED_LHS_WIDTH_PX;
  protected accessor isExpanded_: boolean = false;
  protected accessor isExpandedReady_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    if (!this.expandedLhsUrl_) {
      return;
    }
    this.eventTracker_.add(window, 'message', this.onMessage_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected getExpandedLhsUrl_(): string {
    if (!this.expandedLhsUrl_ || this.isDark === undefined) {
      return this.expandedLhsUrl_;
    }
    const url = new URL(this.expandedLhsUrl_);
    url.searchParams.set('cs', this.isDark ? '1' : '0');
    return url.toString();
  }

  protected onFoldedClick_(): void {
    this.isExpanded_ = true;
  }

  private onMessage_ = (event: MessageEvent) => {
    const expectedOrigin = new URL(this.expandedLhsUrl_).origin;
    if (event.origin !== expectedOrigin) {
      return;
    }

    const expanded = this.shadowRoot.querySelector<HTMLElement>('#expanded');
    const iframe = expanded?.shadowRoot?.querySelector('iframe');
    if (!expanded || !iframe || event.source !== iframe.contentWindow) {
      return;
    }

    const data = event.data as SearchToChromeLhsMessage | null;
    if (typeof data !== 'object' || !data) {
      return;
    }

    switch (data.type) {
      case NtpLhsMessageType.NTP_LHS_READY: {
        this.expandedWidth_ = typeof data.width === 'number' && data.width > 0 ?
            data.width :
            DEFAULT_EXPANDED_LHS_WIDTH_PX;
        this.isExpandedReady_ = true;
        this.isExpanded_ = true;
        break;
      }
      case NtpLhsMessageType.NTP_LHS_CLOSE: {
        this.isExpanded_ = false;
        break;
      }
      case NtpLhsMessageType.NTP_LHS_NEW_THREAD: {
        this.fire('open-composebox');
        break;
      }
      case NtpLhsMessageType.NTP_LHS_NAVIGATE: {
        let url: URL;
        try {
          url = new URL(data.url);
        } catch {
          return;
        }
        if (url.protocol !== 'http:' && url.protocol !== 'https:') {
          return;
        }
        WindowProxy.getInstance().navigate(data.url);
        break;
      }
      default:
        break;
    }
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-left-hand-side': LeftHandSideElement;
  }
}

customElements.define(LeftHandSideElement.is, LeftHandSideElement);

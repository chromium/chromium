// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';

const DEFAULT_EXPANDED_LHS_WIDTH_PX = 261;

enum NtpLhsMessageType {
  READY = 'NTP_LHS_READY',
}

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
      isExpandedReady_: {type: Boolean},
    };
  }

  protected accessor enableThreadsRailHybrid_: boolean =
      loadTimeData.getBoolean('enableThreadsRailHybrid');
  protected accessor expandedLhsUrl_: string =
      loadTimeData.getString('expandedLhsUrl');
  protected accessor expandedWidth_: number = DEFAULT_EXPANDED_LHS_WIDTH_PX;
  protected accessor isExpandedReady_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    const expectedOrigin = new URL(this.expandedLhsUrl_).origin;
    this.eventTracker_.add(window, 'message', (event: MessageEvent) => {
      const data = event.data;
      if (typeof data !== 'object' || !data) {
        return;
      }
      if (event.origin === expectedOrigin &&
          data.type === NtpLhsMessageType.READY) {
        this.expandedWidth_ = typeof data.width === 'number' && data.width > 0 ?
            data.width :
            DEFAULT_EXPANDED_LHS_WIDTH_PX;
        this.isExpandedReady_ = true;
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-left-hand-side': LeftHandSideElement;
  }
}

customElements.define(LeftHandSideElement.is, LeftHandSideElement);

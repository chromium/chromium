// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory, MessageDirection} from './browser_actuator_internals.mojom-webui.js';
import type {SessionSummary} from './browser_actuator_internals.mojom-webui.js';

// TODO(crbug.com/567639192): Truncate only individual `proto_payload` values in
// the backend so transport envelope metadata (sequence numbers, capabilities,
// and subsequent typed_payloads) is always preserved.
const TRUNCATION_SIZE = '8 KB';
export const TRUNCATED_LABEL = `(truncated at ${TRUNCATION_SIZE})`;
export const TRUNCATED_NOTE =
    `Message truncated by the browser at ${TRUNCATION_SIZE}. ` +
    'The text above is incomplete.';

export interface BrowserActuatorInternalsAppElement {
  $: {
    copyToast: CrToastElement,
  };
}

export class BrowserActuatorInternalsAppElement extends CrLitElement {
  static get is() {
    return 'browser-actuator-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      sessions_: {type: Array},
    };
  }

  protected accessor sessions_: SessionSummary[] = [];

  override connectedCallback() {
    super.connectedCallback();
    browserProxyFactory.getInstance().handler.getSessionHistory().then(
        ({sessions}) => {
          this.sessions_ = sessions;
        });
  }

  protected getEventsSummary_(session: SessionSummary): string {
    if (session.events.length < session.totalEvents) {
      return `Events (showing ${session.events.length} of ${
          session.totalEvents})`;
    }
    return `Events (${session.events.length})`;
  }

  protected getDirectionLabel_(direction: MessageDirection): string {
    return direction === MessageDirection.kDownstream ? 'Downstream' :
                                                        'Upstream';
  }

  protected getDirectionClass_(direction: MessageDirection): string {
    return direction === MessageDirection.kDownstream ? 'downstream' :
                                                        'upstream';
  }

  protected formatTime_(time: Date|null): string {
    if (!time || Number.isNaN(time.getTime())) {
      return 'N/A';
    }
    return time.toISOString();
  }

  protected async onExportClick_() {
    const sessionIds = this.sessions_.map(session => session.sessionId);
    const {json} =
        await browserProxyFactory.getInstance().handler.exportSessionHistory(
            sessionIds);
    if (!json) {
      return;
    }
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    this.downloadJson_(json, `browser-actuator-sessions-${timestamp}.json`);
  }

  protected async onCopyClick_() {
    const sessionIds = this.sessions_.map(session => session.sessionId);
    const {json} =
        await browserProxyFactory.getInstance().handler.exportSessionHistory(
            sessionIds);
    if (!json) {
      return;
    }
    await navigator.clipboard.writeText(json);
    await this.$.copyToast.show();
  }

  private downloadJson_(json: string, filename: string) {
    const blob = new Blob([json], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'browser-actuator-internals-app': BrowserActuatorInternalsAppElement;
  }
}

customElements.define(
    BrowserActuatorInternalsAppElement.is, BrowserActuatorInternalsAppElement);

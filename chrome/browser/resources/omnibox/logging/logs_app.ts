// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {getCss} from './logs_app.css.js';
import {getHtml} from './logs_app.html.js';
import {BrowserProxy} from './logs_browser_proxy.js';

/**
 * Converts a mojo time to a JS time.
 */
function convertMojoTimeToJs(mojoTime: Time): Date {
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Get a Chromium source link given a file name and line number.
 */
function getSourceLinkUrl(sourceFile: string, sourceLine: number): string {
  return `https://source.chromium.org/chromium/chromium/src/+/main:${
      sourceFile.slice(6)};l=${sourceLine}`;
}

/**
 * Get the text for a Chromium source link URL.
 */
function getSourceLinkText(sourceFile: string, sourceLine: number): string {
  if (!sourceFile.startsWith('../../')) {
    return `${sourceFile}(${sourceLine})`;
  }
  const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
  if (fileName.length === 0) {
    return `${sourceFile}(${sourceLine})`;
  }
  return `${fileName} (${sourceLine})`;
}

interface EventLogMessage {
  eventTime: Date;
  tag: string;
  sourceLinkText: string;
  sourceLinkUrl: string;
  message: string;
}

export class LogsAppElement extends CrLitElement {
  static get is() {
    return 'logs-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      eventLogMessages_: {type: Array},
    };
  }

  protected accessor eventLogMessages_: EventLogMessage[] = [];

  private proxy_: BrowserProxy = BrowserProxy.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ =
        [this.proxy_.getCallbackRouter().onLogMessageAdded.addListener(
            this.onLogMessageAdded_.bind(this))];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.getCallbackRouter().removeListener(id));
    this.listenerIds_ = [];
  }

  private onLogMessageAdded_(
      eventTime: Time, tag: string, sourceFile: string, sourceLine: number,
      message: string) {
    this.eventLogMessages_.push({
      eventTime: convertMojoTimeToJs(eventTime),
      tag,
      sourceLinkText: getSourceLinkText(sourceFile, sourceLine),
      sourceLinkUrl: getSourceLinkUrl(sourceFile, sourceLine),
      message,
    });
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'logs-app': LogsAppElement;
  }
}

customElements.define(LogsAppElement.is, LogsAppElement);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './event_log.css.js';
import {getHtml} from './event_log.html.js';

/**
 * Converts a mojo time to a JS time.
 */
function convertMojoTimeToJs(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

export class EventLogMessage {
  eventTime: Date;
  sourceLinkText: string;
  sourceLinkURL: string;
  message: string;

  constructor(
      eventTime: Time, sourceFile: string, sourceLine: number,
      message: string) {
    this.eventTime = convertMojoTimeToJs(eventTime);
    this.message = message;
    this.setSourceLink(sourceFile, sourceLine);
  }

  setSourceLink(sourceFile: string, sourceLine: number) {
    if (!sourceFile.startsWith('../../')) {
      this.sourceLinkText = `${sourceFile}(${sourceLine})`;
      return;
    }
    const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
    if (fileName.length === 0) {
      this.sourceLinkText = `${sourceFile}(${sourceLine})`;
      return;
    }
    this.sourceLinkText = `${fileName}(${sourceLine})`;
    this.sourceLinkURL =
        `https://source.chromium.org/chromium/chromium/src/+/main:${
            sourceFile.slice(6)};l=${sourceLine}`;
  }

  /**
   * Returns a string for dumping the message to logs.
   */
  toLogDump() {
    return `${this.eventTime}  ${this.sourceLinkText} ${this.message}`;
  }
}

export class OnDeviceInternalsEventLogElement extends CrLitElement {
  static get is() {
    return 'on-device-internals-event-log';
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

  protected eventLogMessages_: EventLogMessage[] = [];

  override connectedCallback() {
    super.connectedCallback();
    BrowserProxy.getInstance().callbackRouter.onLogMessageAdded.addListener(
        this.onLogMessageAdded_.bind(this));
  }

  /**
   * The callback to save the logs to a file.
   */
  protected onEventLogsDumpClick_() {
    const data =
        this.eventLogMessages_.map(message => message.toLogDump()).join('\r\n');
    const blob = new Blob([data], {'type': 'text/json'});
    const url = URL.createObjectURL(blob);
    const filename = 'optimization_guide_internals_logs_dump.json';

    const a = document.createElement('a');
    a.setAttribute('href', url);
    a.setAttribute('download', filename);

    const event = new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      view: window,
    });
    a.dispatchEvent(event);
  }

  private onLogMessageAdded_(
      eventTime: Time, sourceFile: string, sourceLine: number,
      message: string) {
    this.eventLogMessages_.push(
        new EventLogMessage(eventTime, sourceFile, sourceLine, message));
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-event-log': OnDeviceInternalsEventLogElement;
  }
}

customElements.define(
    OnDeviceInternalsEventLogElement.is, OnDeviceInternalsEventLogElement);

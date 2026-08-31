// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {browserProxyFactory} from './web_app_internals.mojom-webui.js';
import type {BrowserProxy} from './web_app_internals.mojom-webui.js';
import type {AppIndexEntry, DebugData} from './web_app_internals_utils.js';
import {debugDataJsonReplacer, filterToApp, getAppIndexEntries, getQuery} from './web_app_internals_utils.js';

export class WebAppInternalsAppElement extends CrLitElement {
  static get is() {
    return 'web-app-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      rawDebugData_: {type: String},
      debugInfoJson_: {type: String},
      parsedDebugData_: {type: Object},
      query_: {type: String},
      isIwaPolicyInstallEnabled_: {type: Boolean},
    };
  }

  protected accessor rawDebugData_: string = '';
  protected accessor debugInfoJson_: string = '';
  protected accessor parsedDebugData_: DebugData|null = null;
  protected accessor query_: string = '';
  protected accessor isIwaPolicyInstallEnabled_: boolean =
      loadTimeData.getBoolean('isIwaPolicyInstallEnabled');

  private tracker_: EventTracker = new EventTracker();
  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.query_ = getQuery();
    this.tracker_.add(window, 'hashchange', () => {
      this.query_ = getQuery();
    });

    this.fetchDebugInfo_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.tracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('parsedDebugData_') ||
        changedPrivateProperties.has('rawDebugData_')) {
      this.debugInfoJson_ = this.parsedDebugData_ ?
          JSON.stringify(this.parsedDebugData_, debugDataJsonReplacer, 2) :
          this.rawDebugData_;
    }
  }

  private async fetchDebugInfo_(): Promise<void> {
    try {
      const response =
          await this.browserProxy_.handler.getDebugInfoAsJsonString();
      this.rawDebugData_ = response.result;
      try {
        this.parsedDebugData_ = JSON.parse(response.result);
      } catch {
        this.parsedDebugData_ = null;
      }
    } catch (e) {
      console.warn('Failed to fetch debug info', e);
    }
  }

  protected getAppIndexEntries_(): AppIndexEntry[] {
    if (!this.parsedDebugData_) {
      return [];
    }
    return getAppIndexEntries(this.parsedDebugData_, this.query_);
  }

  protected getFormattedJson_(): string {
    if (!this.parsedDebugData_) {
      return this.debugInfoJson_;
    }
    if (this.query_) {
      const displayData = filterToApp(this.parsedDebugData_, this.query_);
      return JSON.stringify(displayData, debugDataJsonReplacer, 2);
    }
    return this.debugInfoJson_;
  }

  protected onDownloadButtonClick_() {
    const url = URL.createObjectURL(new Blob([this.debugInfoJson_], {
      type: 'application/json',
    }));

    const a = document.createElement('a');
    a.href = url;
    a.download = 'web_app_internals.json';
    a.click();
    URL.revokeObjectURL(url);
  }

  protected onCopyButtonClick_(): void {
    navigator.clipboard.writeText(this.debugInfoJson_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'web-app-internals-app': WebAppInternalsAppElement;
  }
}

customElements.define(WebAppInternalsAppElement.is, WebAppInternalsAppElement);

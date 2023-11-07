// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DlpEvent, PageHandler, PageHandlerInterface, ReportingObserverReceiver} from './dlp_internals.mojom-webui.js';
import {getTemplate} from './dlp_reporting_tab.html.js';

export class DlpReportingElement extends CustomElement {
  static get is() {
    return 'dlp-reporting-tab';
  }

  static override get template() {
    return getTemplate();
  }

  private pageHandler_: PageHandlerInterface;
  private readonly reportingObserver_: ReportingObserverReceiver;

  constructor() {
    super();

    this.pageHandler_ = PageHandler.getRemote();
    this.reportingObserver_ = new ReportingObserverReceiver(this);
    this.pageHandler_.observeReporting(
        this.reportingObserver_.$.bindNewPipeAndPassRemote());
  }

  /** Implements ReportingObserverInterface */
  onReportEvent(event: DlpEvent): void {
    // TODO(ayaelattar): Show it in the html page.
    console.warn(JSON.stringify(event));
  }
}

customElements.define(DlpReportingElement.is, DlpReportingElement);

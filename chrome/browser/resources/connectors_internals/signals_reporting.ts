// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {BrowserProxy} from './browser_proxy.js';
import type {PageHandlerInterface, SignalsReportingState} from './connectors_internals.mojom-webui.js';
import {getTemplate} from './signals_reporting.html.js';

export class SignalsReportingElement extends CustomElement {
  static get is() {
    return 'signals-reporting';
  }

  static override get template() {
    return getTemplate();
  }

  set errorText(errorText: string) {
    this.setValueToElement('#error-text', errorText);
  }

  set statusReportEnabledString(isEnabledString: string) {
    this.setValueToElement('#status-reporting-enabled', isEnabledString);
  }

  set signalsReportEnabledString(isEnabledString: string) {
    this.setValueToElement('#signals-reporting-enabled', isEnabledString);
  }

  set lastUploadAttemptTimestamp(timestamp: string) {
    this.setValueToElement('#last-upload-attempt-timestamp', timestamp);
  }

  set lastUploadSuccessTimestamp(timestamp: string) {
    this.setValueToElement('#last-upload-success-timestamp', timestamp);
  }

  set lastSignalsUploadConfigString(config: string) {
    this.setValueToElement('#last-signals-upload-config', config);
  }

  set canCollectAllFieldsString(canCollectAllFields: string) {
    this.setValueToElement('#can-collect-all', canCollectAllFields);
  }

  private get pageHandler(): PageHandlerInterface {
    return BrowserProxy.getInstance().handler;
  }

  constructor() {
    super();

    this.fetchSignalsReportingState();
  }

  private updateState(state: SignalsReportingState|undefined) {
    if (!state) {
      this.errorText = 'Can\'t retrieve signals reporting state.';
      this.showErrorElement();
      return;
    }

    if (state.errorInfo) {
      this.errorText = state.errorInfo;
      this.showErrorElement();
    } else {
      this.hideErrorElement();
    }

    this.statusReportEnabledString = `${state.statusReportEnabled}`;
    this.signalsReportEnabledString = `${state.signalsReportEnabled}`;
    this.lastUploadAttemptTimestamp = state.lastUploadAttemptTimestamp;
    this.lastUploadSuccessTimestamp = state.lastUploadSuccessTimestamp;
    this.lastSignalsUploadConfigString = state.lastSignalsUploadConfig;
    this.canCollectAllFieldsString = `${state.canCollectAllFields}`;
  }

  private fetchSignalsReportingState() {
    this.pageHandler.getSignalsReportingState().then(
        (response: {state: SignalsReportingState}) =>
            this.updateState(response?.state),
        err => console.error(
            `fetchSignalsReportingState failed: ${JSON.stringify(err)}`));
  }

  private showErrorElement() {
    this.getRequiredElement('#error-section')?.classList.remove('hidden');
  }

  private hideErrorElement() {
    this.getRequiredElement('#error-section')?.classList.add('hidden');
  }

  private setValueToElement(elementId: string, stringValue: string) {
    const htmlElement = (this.$(elementId) as HTMLElement);
    if (htmlElement) {
      htmlElement.innerText = stringValue;
    } else {
      console.error(`Could not find ${elementId} element.`);
    }
  }
}

customElements.define(SignalsReportingElement.is, SignalsReportingElement);

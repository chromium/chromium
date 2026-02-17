// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_page.css.js';
import {getHtml} from './error_page.html.js';

export enum ErrorType {
  GLIC_NOT_ENABLED = 'glic-not-enabled',
  NO_SEARCH_RESULTS = 'no-search-results',
}

export class ErrorPageElement extends CrLitElement {
  static get is() {
    return 'error-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      errorType: {type: String},
    };
  }
  accessor errorType: ErrorType = ErrorType.GLIC_NOT_ENABLED;

  protected shouldShowErrorIcon(): boolean {
    return this.errorType === ErrorType.GLIC_NOT_ENABLED;
  }

  protected errorTitle(): string {
    switch (this.errorType) {
      case ErrorType.GLIC_NOT_ENABLED:
        return loadTimeData.getString('errorPageTitle');
      case ErrorType.NO_SEARCH_RESULTS:
        return loadTimeData.getString('noSearchResultsTitle');
      default:
        assertNotReached();
    }
  }

  protected errorDescription(): string {
    switch (this.errorType) {
      case ErrorType.GLIC_NOT_ENABLED:
        return loadTimeData.getString('errorPageDescription');
      case ErrorType.NO_SEARCH_RESULTS:
        return loadTimeData.getString('noSearchResultsDescription');
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'error-page': ErrorPageElement;
  }
}

customElements.define(ErrorPageElement.is, ErrorPageElement);

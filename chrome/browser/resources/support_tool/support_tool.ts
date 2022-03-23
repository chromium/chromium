// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './data_collectors.js';
import './issue_details.js';
import './spinner_page.js';
import './support_tool_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DataCollectorsElement} from './data_collectors.js';
import {IssueDetailsElement} from './issue_details.js';
import {SpinnerPageElement} from './spinner_page.js';
import {getTemplate} from './support_tool.html.js';

enum SupportToolPageIndex {
  ISSUE_DETAILS,
  DATA_COLLECTOR_SELECTION,
  SPINNER,
}

export interface SupportToolElement {
  $: {
    issueDetails: IssueDetailsElement,
    dataCollectors: DataCollectorsElement,
    spinnerPage: SpinnerPageElement,
  };
}

export class SupportToolElement extends PolymerElement {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: {
        type: SupportToolPageIndex,
        value: SupportToolPageIndex.ISSUE_DETAILS,
      },
      supportToolPageIndex_: {
        readonly: true,
        type: Object,
        value: SupportToolPageIndex,
      },
    };
  }

  private selectedPage_: SupportToolPageIndex;

  private onContinueClick_() {
    this.selectedPage_ = this.selectedPage_ + 1;
    // TODO(b/219730597): If selected page is data collections page, send signal
    // to chrome using BrowserProxy to start data collection with the data we
    // gathered in IssueDetailsElement and DataCollectorsElement. This part will
    // be added in follow-up CL.
  }

  private onBackClick_() {
    this.selectedPage_ = this.selectedPage_ - 1;
  }

  private shouldHideBackButton_(): boolean {
    // Back button will only be shown on data collectors selection page.
    return this.selectedPage_ !== SupportToolPageIndex.DATA_COLLECTOR_SELECTION;
  }

  private shouldHideContinueButtonContainer_(): boolean {
    // Continue button container will only be shown in issue details page and
    // data collectors selection page.
    return this.selectedPage_ >= SupportToolPageIndex.SPINNER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'support-tool': SupportToolElement;
  }
}

customElements.define(SupportToolElement.is, SupportToolElement);
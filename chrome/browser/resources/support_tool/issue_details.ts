// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import './support_tool_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy, IssueDetails} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './issue_details.html.js';
import {SupportToolPageMixin} from './support_tool_page_mixin.js';

const IssueDetailsElementBase = SupportToolPageMixin(PolymerElement);

export class IssueDetailsElement extends IssueDetailsElementBase {
  static get is() {
    return 'issue-details';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      caseId_: {
        type: String,
        value: () => loadTimeData.getString('caseId'),
      },
      emails_: {
        type: Array,
        value: () => [],
      },
      issueDescription_: {
        type: String,
        value: '',
      },
      selectedEmail_: {
        type: String,
        value: '',
      },
    };
  }

  private caseId_: string;
  private emails_: string[] = [this.i18n('dontIncludeEmailAddress')];
  private issueDescription_: string;
  private selectedEmail_: string;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();


  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getEmailAddresses().then((emails: string[]) => {
      this.emails_ = emails;
      // Add default email at the end of emails list for user to be able to
      // choose to not include email address.
      this.emails_.push(this.i18n('dontIncludeEmailAddress'));
    });
  }

  getIssueDetails(): IssueDetails {
    return {
      caseId: this.caseId_,
      // Set emailAddress field to empty string if user selected to not include
      // email address.
      emailAddress:
          (this.selectedEmail_ === this.i18n('dontIncludeEmailAddress')) ?
          '' :
          this.selectedEmail_,
      issueDescription: this.issueDescription_,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'issue-details': IssueDetailsElement;
  }
}

customElements.define(IssueDetailsElement.is, IssueDetailsElement);

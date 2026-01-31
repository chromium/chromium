// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy, IssueDetails} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './issue_details.css.js';
import {getHtml} from './issue_details.html.js';
import {SupportToolPageMixinLit} from './support_tool_page_mixin_lit.js';

const IssueDetailsElementBase = SupportToolPageMixinLit(CrLitElement);

export class IssueDetailsElement extends IssueDetailsElementBase {
  static get is() {
    return 'issue-details';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      caseId_: {type: String},
      emails_: {type: Array},
      issueDescription_: {type: String},
      selectedEmail_: {type: String},
    };
  }

  protected accessor caseId_: string = loadTimeData.getString('caseId');
  protected accessor emails_: string[] =
      [loadTimeData.getString('dontIncludeEmailAddress')];
  protected accessor issueDescription_: string = '';
  protected accessor selectedEmail_: string = '';
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

  protected onSelectedEmailChange_(e: Event) {
    this.selectedEmail_ = (e.target as HTMLSelectElement).value;
  }

  protected onIssueDescriptionInput_(e: Event) {
    this.issueDescription_ = (e.target as HTMLTextAreaElement).value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'issue-details': IssueDetailsElement;
  }
}

customElements.define(IssueDetailsElement.is, IssueDetailsElement);

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import './icons.html.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './report_unsafe_site_app.css.js';
import {getHtml} from './report_unsafe_site_app.html.js';
import {ReportUnsafeSiteBrowserProxyImpl} from './report_unsafe_site_browser_proxy.js';

export interface ReportUnsafeSiteAppElement {
  $: {
    includeScreenshotCheckbox: CrCheckboxElement,
  };
}

export class ReportUnsafeSiteAppElement extends CrLitElement {
  static get is() {
    return 'report-unsafe-site-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageUrl_: {type: String},
      includeScreenshot_: {type: Boolean},
      screenshotDataUri_: {type: String},
      isSendingCsdPing_: {type: Boolean},
    };
  }

  protected accessor pageUrl_: string = '';
  protected accessor includeScreenshot_: boolean = false;
  protected accessor screenshotDataUri_: string = '';
  protected accessor isSendingCsdPing_: boolean = false;

  override async connectedCallback() {
    super.connectedCallback();
    const pageHandler =
        ReportUnsafeSiteBrowserProxyImpl.getInstance().getPageHandler();
    const pageInfo = await pageHandler.getTriggeringPageInfo();
    this.pageUrl_ = pageInfo.pageUrl;
    this.screenshotDataUri_ = pageInfo.screenshotDataUri;
    this.includeScreenshot_ = (this.screenshotDataUri_.length > 0);
  }

  override firstUpdated() {
    const pageHandler =
        ReportUnsafeSiteBrowserProxyImpl.getInstance().getPageHandler();
    pageHandler.showUi();
  }

  protected onIncludeScreenshotCheckedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.includeScreenshot_ = e.detail.value;
  }

  protected async onActionButtonClick_() {
    this.isSendingCsdPing_ = true;
    const pageHandler =
        ReportUnsafeSiteBrowserProxyImpl.getInstance().getPageHandler();
    await pageHandler.sendReport(this.includeScreenshot_);
    pageHandler.closeDialog();
    this.isSendingCsdPing_ = false;
  }

  protected onCancelButtonClick_() {
    ReportUnsafeSiteBrowserProxyImpl.getInstance()
        .getPageHandler()
        .closeDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'report-unsafe-site-app': ReportUnsafeSiteAppElement;
  }
}

customElements.define(
    ReportUnsafeSiteAppElement.is, ReportUnsafeSiteAppElement);

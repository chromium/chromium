// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './report_unsafe_site_app.css.js';
import {getHtml} from './report_unsafe_site_app.html.js';
import {ReportUnsafeSiteBrowserProxyImpl} from './report_unsafe_site_browser_proxy.js';

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
      pageUrl_: {
        type: String,
      },
    };
  }

  protected accessor pageUrl_: string = '';

  override async connectedCallback() {
    super.connectedCallback();
    this.pageUrl_ = (await ReportUnsafeSiteBrowserProxyImpl.getInstance()
                         .getPageHandler()
                         .getPageUrl())
                        .pageUrl;
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

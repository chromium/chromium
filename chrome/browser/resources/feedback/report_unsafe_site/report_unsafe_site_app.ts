// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './report_unsafe_site_app.html.js';

export class ReportUnsafeSiteAppElement extends CrLitElement {
  static get is() {
    return 'report-unsafe-site-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'report-unsafe-site-app': ReportUnsafeSiteAppElement;
  }
}

customElements.define(
    ReportUnsafeSiteAppElement.is, ReportUnsafeSiteAppElement);

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NtpExtensionAppElement} from './app.js';

export function getHtml(this: NtpExtensionAppElement) {
  return html`
<div id="content">
  <ntp-logo id="logo" ?single-colored="${this.isLogoSingleColored_()}"
      .theme="${this.theme_}">
  </ntp-logo>
  <div id="searchboxContainer">
    <ntp-searchbox id="searchbox" ?shown="${!!this.theme_}"
        ?is-dark="${this.isThemeDark_()}"
        placeholder-text="${this.i18n('searchBoxPlaceholder')}">
    </ntp-searchbox>
  </div>
</div>`;
}
